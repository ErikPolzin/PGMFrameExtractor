#include <iostream>
#include <string.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <malloc.h>
#include "extractor.h"

namespace PLZERI001 {

Frame::Frame(int w, int h, PGMMetadata * imd):
    inputMdata(imd), width(w), height(h), x(-1), y(-1), inverted(false)
{
    data = new char*[height];
    for (int i = 0; i < height; ++i) data[i] = new char[width];
};

Frame::~Frame()
{
    for (int i = 0; i < this->height; ++i) delete data[i];
    delete [] data;
};

void Frame::setOrigin(int _x, int _y) {x = _x; y = _y;};

void Frame::setInverted(bool inv) {inverted=inv;};

std::ostream &operator<<(std::ostream& stream, const Frame& f)
{
    for (int r = 0; r < f.height; ++r)
    {
        for (int c = 0; c < f.width; ++c)
        {
            if (f.inverted) stream << char(255 - f.data[r][c]);
            else stream << f.data[r][c];
        }
    }
    return stream;
}

std::ifstream &operator>>(std::ifstream& stream, Frame& f)
{
    const int maxpos = f.inputMdata->file_height*f.inputMdata->file_width;
    int pos = f.inputMdata->data_offset+f.y*f.inputMdata->file_width+f.x;
    for (int i = 0; i < f.height; ++i)
    {
        pos = pos % maxpos;
        stream.seekg(pos);
        stream.read(f.data[i], f.width);
        pos += f.inputMdata->file_width;
        if (pos > maxpos) pos = pos - maxpos;
    }
    return stream;
}

std::ostream &operator<<(std::ostream& stream, const PGMMetadata& md)
{
    stream << "P5" << std::endl << md.file_width << ' ' << md.file_height 
        << std::endl << "255" << std::endl;
    return stream;
}

std::ifstream &operator>>(std::ifstream& stream, PGMMetadata& md)
{
    std::string line;
    std::getline(stream, line);
    while (stream.peek() == '#') std::getline(stream, line);
    stream >> md.file_width;
    stream >> md.file_height >> std::ws;
    std::getline(stream, line);
    md.data_offset = stream.tellg();
    return stream;
}

std::string OutputSpec::file_name (int frame_index)
{
    if (operation & REVOP) frame_index = -frame_index;
    std::stringstream fn;
    fn << name << "-" << std::setfill('0') << std::setw(5) << frame_index << ".pgm";
    return fn.str();
}
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Specify path to PGM file" << std::endl;
        return 1;
    }
    std::vector<int> points_array;
    std::vector<PLZERI001::OutputSpec> outputs;
    std::ifstream inputFile(argv[1], std::ios::binary);
    std::string dir = "";
    int framew, frameh, fileh, filew;

    if (!inputFile)
    {
        std::cout << "Cannot open file at " << argv[1] << std::endl;
        return 1;
    }
    std::string fp;
    PLZERI001::OutputSpec output;
    PLZERI001::PGMMetadata input_meta;
    PLZERI001::PGMMetadata output_meta(framew, frameh);
    inputFile >> input_meta;
    PLZERI001::Frame frame = PLZERI001::Frame(framew, frameh, &input_meta);
    int x1, x2, y1, y2;
    double x, y, dx, dy, spd, tot_dist, dist, prog;
    int fileidx = 0; int nframes = 0;
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i=0; i<points_array.size()/2-1; i++)
    {
        x1 = points_array[i*2]; y1 = points_array[i*2+1];
        x2 = points_array[i*2+2]; y2 = points_array[i*2+3];
        tot_dist = std::sqrt(std::pow(x2-x1, 2)+std::pow(y2-y1, 2));
        x = x1; y = y1;
        dx = double(x2-x1)/tot_dist; dy = double(y2-y1)/tot_dist;
        do {
            dist = std::sqrt(std::pow(x-x1, 2)+std::pow(y-y1, 2));
            prog = dist / tot_dist;
            spd = prog*(1-prog)+1;
            frame.setOrigin(x, y);
            try {
                inputFile >> frame;
            } catch (std::bad_alloc& e) {
                std::cout << "Unable to alloc. memory for frame " << i << std::endl;
                continue;
            }
            for (int k = 0; k < outputs.size(); k++)
            {
                output = outputs[k];
                fp = output.file_name(fileidx++);
                if (!dir.empty()) fp = dir + "/" + fp;
                std::ofstream outfile(fp, std::ios::binary);
                if (!outfile)
                {
                    std::cout << "Unable to open output file at " << fp << std::endl;
                    continue;
                }
                outfile << output_meta;
                frame.setInverted(output.operation & INVOP);
                outfile << frame;
                outfile.close();
                nframes++;
            }
            std::cout << "Wrote file " << nframes << "\r";
            x+=dx*spd; y+=dy*spd;
        } while (dist<tot_dist && nframes <= MAX_FRAMES);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "Wrote " << nframes << " frames in " << ms_int.count() << "ms\n";
    std::cout << "Malloc stats:\n";
    malloc_stats();
    return 0;
}
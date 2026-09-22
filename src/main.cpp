#include <fstream>
#include <iostream>

int main() {
    const int width = 256;
    const int height = 256;

    std::ofstream out("output.ppm");
    out << "P3\n" << width << " " << height << "\n255\n";

    for (int j = height - 1; j >= 0; --j) {
        for (int i = 0; i < width; ++i) {
            double r = double(i) / (width - 1);
            double g = double(j) / (height - 1);
            double b = 0.25;

            int ir = static_cast<int>(255.999 * r);
            int ig = static_cast<int>(255.999 * g);
            int ib = static_cast<int>(255.999 * b);

            out << ir << ' ' << ig << ' ' << ib << '\n';
        }
    }

    out.close();
    std::cout << "Image generated: output.ppm\n";
    return 0;
}
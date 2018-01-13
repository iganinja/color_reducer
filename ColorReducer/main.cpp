#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include "lodepng.h"

using Byte = std::uint8_t;
using Pixel = std::uint32_t; // RGBA
using Color = Pixel;
constexpr unsigned PixelSize = sizeof(Pixel);
using ImageData = std::vector<unsigned char>; // We assume it is in RGBA format
using ImageIt = ImageData::iterator;
using Palette = std::vector<Color>;
using Histogram = std::unordered_map<Color, std::size_t>;

Byte red(Color color)
{
    return (color & 0xFF000000) >> 24;
}

Byte green(Color color)
{
    return (color & 0x00FF0000) >> 16;
}

Byte blue(Color color)
{
    return (color & 0x0000FF00) >> 8;
}

Byte alpha(Color color)
{
    return color & 0x000000FF;
}

Pixel getPixel(ImageIt it)
{
    const std::uint32_t r = *(it + 0);
    const std::uint32_t g = *(it + 1);
    const std::uint32_t b = *(it + 2);
    const std::uint32_t a = *(it + 3);
    return r << 24 | g << 16 | b << 8 | a;
}

void setPixel(ImageIt it, Color color)
{
    *(it + 0) = red(color);
    *(it + 1) = green(color);
    *(it + 2) = blue(color);
    *(it + 3) = alpha(color);
}

Palette loadPalette(const std::string& paletteFileName)
{
    ImageData image;
    unsigned imageWidth, imageHeight;
    const auto error = lodepng::decode(image, imageWidth, imageHeight, paletteFileName);

    if(error)
    {
        throw std::runtime_error{"Cannot open " + paletteFileName + " file (" + std::to_string(error) + "): " + std::string{lodepng_error_text(error)}};
    }

    std::unordered_set<Pixel> uniqueColors;

    for(auto it = image.begin(); it != image.end(); it += PixelSize)
    {
        const auto pixel = getPixel(it);
        if(uniqueColors.find(getPixel(it)) == uniqueColors.end())
        {
            uniqueColors.insert(pixel);
        }
    }

    std::cout << "Created palette from " << paletteFileName << " file: " << uniqueColors.size() << " unique colors\n";

    return Palette{uniqueColors.begin(), uniqueColors.end()};
}

float colorSquareDistance(Color c1, Color c2)
{
    const auto redDifference = red(c1) - red(c2);
    const auto greenDifference = green(c1) - green(c2);
    const auto blueDifference = blue(c1) - blue(c2);
    const auto alphaDifference = alpha(c1) * alpha(c2);
    return redDifference * redDifference + greenDifference * greenDifference + blueDifference * blueDifference + alphaDifference * alphaDifference;
}

Color findClosestColor(Color color, const Palette& palette)
{
    Color closestColor = color;
    float distance = std::numeric_limits<float>::max();

    for(const auto paletteColor : palette)
    {
        const auto d = colorSquareDistance(color, paletteColor);
        if(d < distance)
        {
            distance = d;
            closestColor = paletteColor;
        }
    }

    return closestColor;
}

auto replaceColorsByClosestOne = [](ImageData& image, const Palette& palette, auto onColorCacheMiss, auto onColorCacheHit)
{
    std::unordered_map<Color, Color> colorMap;

    for(auto imageIt = image.begin(); imageIt != image.end(); imageIt += PixelSize)
    {
        const auto color = getPixel(imageIt);

        auto colorMapIt = colorMap.find(color);
        if(colorMapIt == colorMap.end())
        {
            const auto closestColor = findClosestColor(color, palette);
            colorMap[color] = closestColor;
            setPixel(imageIt, closestColor);
            onColorCacheMiss(closestColor);
        }
        else
        {
            const auto closestColor = (*colorMapIt).second;
            setPixel(imageIt, closestColor);
            onColorCacheHit(closestColor);
        }
    }
};


Histogram convertImageToPaletteAndGetHistogram(ImageData& image, const Palette& palette)
{
    std::unordered_map<Color, std::size_t> histogram;

    replaceColorsByClosestOne(image,
                palette,
                [&histogram](auto color)
                {
                    histogram[color] = 1;
                },
                [&histogram](auto color)
                {
                    histogram[color] ++;
                });

    return histogram;
}

void reduceColors(ImageData& image, const Histogram& histogram, std::size_t maximumColorNumber)
{
    struct ColorFrequency
    {
        Color color;
        std::size_t count;
    };
    std::vector<ColorFrequency> colorFrequency;
    colorFrequency.reserve(histogram.size());

    for(const auto& pair : histogram)
    {
        colorFrequency.push_back(ColorFrequency{pair.first, pair.second});
    }

    // Sort in decreasing way in function of color's counting
    std::sort(colorFrequency.begin(), colorFrequency.end(), [](const auto& v1, const auto& v2)
    {
        return v1.count > v2.count;
    });

    if(colorFrequency.size() > maximumColorNumber)
    {
        colorFrequency.resize(maximumColorNumber);
    }

    Palette reducedPalette;
    reducedPalette.reserve(colorFrequency.size());

    for(const auto colorCounting : colorFrequency)
    {
        reducedPalette.push_back(colorCounting.color);
    }

    replaceColorsByClosestOne(image, reducedPalette, [](auto){}, [](auto){});
}

void saveImage(const std::string& fileName, const ImageData& imageData, int width, int height)
{
    const auto error = lodepng::encode(fileName, imageData, width, height);

    if(error)
    {
        throw std::runtime_error{"Cannot save " + fileName + " file (" + std::to_string(error) + "): " + std::string{lodepng_error_text(error)}};
    }
}


int main(int argumentNumber, char *arguments[])
{
    if(argumentNumber != 5)
    {
        std::cout << "color_reducer 1.0\n";
        std::cout << "Takes an input image, a palette image and creates a new image with palette's closest colors using provided maximum number of them\n";
        std::cout << "Usage: color_reducer input_file_name palette_image_file maximum_color_number output_file_name\n";
        std::cout << "Example: color_reducer main_menu.png sms_palette.png 16 main_menu_16.png\n";

        return 0;
    }

    try
    {
        const std::string inputFileName{arguments[1]};
        const std::string paletteFileName{arguments[2]};
        const std::size_t maximumColorNumber = std::atoi(arguments[3]);
        const std::string outputFileName{arguments[4]};

        ImageData inputImage;
        unsigned inputImageWidth, inputImageHeight;
        const auto error = lodepng::decode(inputImage, inputImageWidth, inputImageHeight, inputFileName);

        if(error)
        {
            throw std::runtime_error{"Cannot open " + inputFileName + " file (" + std::to_string(error) + "): " + std::string{lodepng_error_text(error)}};
        }

        const auto palette = loadPalette(paletteFileName);
        const auto histogram = convertImageToPaletteAndGetHistogram(inputImage, palette);
        reduceColors(inputImage, histogram, maximumColorNumber);
        saveImage(outputFileName, inputImage, inputImageWidth, inputImageHeight);

        std::cout << "Saved " << outputFileName << " file\n";
    }
    catch(const std::exception& exception)
    {
        std::cout << "Error: " << exception.what() << "\n";
    }

    return 0;
}

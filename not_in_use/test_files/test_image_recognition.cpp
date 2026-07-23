#include "brain/research/tools/ImageRecognitionTool.h"
#include <cassert>

int main() {
    yuki::research::ImageRecognitionTool tool;
    std::vector<uint8_t> dummyImg = {0x89, 0x50, 0x4E, 0x47};

    auto ocrText = tool.ocr(dummyImg);
    assert(!ocrText.empty());

    auto objects = tool.detectObjects(dummyImg);
    assert(!objects.empty());

    return 0;
}

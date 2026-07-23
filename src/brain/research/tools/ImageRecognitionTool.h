#ifndef YUKI_IMAGE_RECOGNITION_TOOL_H
#define YUKI_IMAGE_RECOGNITION_TOOL_H

#include "brain/research/core/ToolInterface.h"

namespace yuki {
namespace research {

struct DetectedObject {
    std::string label;
    float confidence = 0.0f;
    float bbox[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

class ImageRecognitionTool : public ToolInterface {
public:
    ToolResult execute(const std::vector<uint8_t>& input) override;
    ToolMetadata getMetadata() const override;

    std::string ocr(const std::vector<uint8_t>& imageBytes);
    std::vector<DetectedObject> detectObjects(const std::vector<uint8_t>& imageBytes);
    std::string describeScene(const std::vector<uint8_t>& imageBytes);
};

} // namespace research
} // namespace yuki

#endif

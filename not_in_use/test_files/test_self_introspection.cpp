#include "brain/introspection/SelfIntrospectionTool.h"
#include <cassert>

int main() {
    yuki::introspection::SelfIntrospectionTool tool;

    auto profile = tool.profileOrgan("MetacognitionEngine");
    assert(profile.organName == "MetacognitionEngine");
    assert(tool.checkIntegrity());

    return 0;
}

#pragma once
#include "MeaningTypes.h"
class RequestClassifier {
public:
    RequestClassifier();
    std::string classify(const MeaningState& state) const;
};

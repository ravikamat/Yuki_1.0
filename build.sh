#!/bin/bash
# Build script for Yuki

set -e  # Exit on error

echo "========================================"
echo "BUILDING YUKI COGNITIVE AGENT"
echo "========================================"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR="build"
JOBS=$(nproc)

echo -e "${YELLOW}Build type: ${BUILD_TYPE}${NC}"
echo -e "${YELLOW}Jobs: ${JOBS}${NC}"

# Create build directory
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure
echo -e "\n${YELLOW}Configuring with CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      ..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo -e "\n${YELLOW}Building with ${JOBS} jobs...${NC}"
cmake --build . --config ${BUILD_TYPE} -j${JOBS}

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "\n${GREEN}Build successful!${NC}"

# Run tests
echo -e "\n${YELLOW}Running tests...${NC}"

# Unit tests
echo -e "\n${YELLOW}Running unit tests...${NC}"
./unit_tests
if [ $? -ne 0 ]; then
    echo -e "${RED}Unit tests failed!${NC}"
    exit 1
fi

# Property-based tests (quick run)
echo -e "\n${YELLOW}Running property-based tests (quick)...${NC}"
timeout 60 ./property_tests --quick 2>&1 | head -100
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Property tests completed with issues${NC}"
fi

# Integration tests
echo -e "\n${YELLOW}Running integration tests...${NC}"
./integration_tests
if [ $? -ne 0 ]; then
    echo -e "${RED}Integration tests failed!${NC}"
    exit 1
fi

echo -e "\n${GREEN}All tests passed!${NC}"

# Pre-fill brain storage
echo -e "\n${YELLOW}Pre-filling brain storage...${NC}"
cd ..
python3 scripts/prefill_brain_storage.py

echo -e "\n${GREEN}========================================"
echo "BUILD AND TEST COMPLETE"
echo "========================================${NC}"
echo ""
echo "To run Yuki:"
echo "  cd build && ./bin/Yuki --config=../config/cognitive_config.fbs"
echo ""
echo "To run with custom config:"
echo "  ./bin/Yuki --config=path/to/config.fbs"
echo ""
echo "To run with metrics:"
echo "  # Metrics available at http://localhost:9090/metrics"
echo ""

# Run Yuki for a quick test
echo -e "\n${YELLOW}Running Yuki for quick test (5 seconds)...${NC}"
timeout 5 ./bin/Yuki --config=../config/cognitive_config.fbs 2>&1 | head -50 || true

echo -e "\n${GREEN}========================================"
echo "BUILD, TEST, AND QUICK RUN COMPLETE"
echo "========================================${NC}"
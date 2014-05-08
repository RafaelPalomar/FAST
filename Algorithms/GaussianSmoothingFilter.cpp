#include "GaussianSmoothingFilter.hpp"
#include "Exception.hpp"
#include "DeviceManager.hpp"
#include "Image.hpp"
#include "DynamicImage.hpp"
using namespace fast;

void GaussianSmoothingFilter::setInput(ImageData::pointer input) {
    mInput = input;
    mIsModified = true;
    addParent(input);
    if(input->isDynamicData()) {
        mTempOutput = DynamicImage::New();
    } else {
        mTempOutput = Image::New();
        input->retain(mDevice);
    }
    mOutput = mTempOutput;
}


void GaussianSmoothingFilter::setDevice(ExecutionDevice::pointer device) {
    mDevice = device;
    mIsModified = true;
    mRecreateMask = true;
}

void GaussianSmoothingFilter::setMaskSize(unsigned char maskSize) {
    if(maskSize % 2 != 1)
        throw Exception("Mask size of GaussianSmoothingFilter must be odd.");

    mMaskSize = maskSize;
    mIsModified = true;
    mRecreateMask = true;
}

void GaussianSmoothingFilter::setStandardDeviation(float stdDev) {
    if(stdDev <= 0)
        throw Exception("Standard deviation of GaussianSmoothingFilter can't be less than 0.");

    mStdDev = stdDev;
    mIsModified = true;
    mRecreateMask = true;
}

ImageData::pointer GaussianSmoothingFilter::getOutput() {
    if(!mInput.isValid()) {
        throw Exception("Must call setInput before getOutput in GaussianSmoothingFilter");
    }
    if(mTempOutput.isValid()) {
        mTempOutput->setParent(mPtr.lock());

        ImageData::pointer newSmartPtr;
        newSmartPtr.swap(mTempOutput);

        return newSmartPtr;
    } else {
        return mOutput.lock();
    }
}

GaussianSmoothingFilter::GaussianSmoothingFilter() {
    mDevice = DeviceManager::getInstance().getDefaultComputationDevice();
    mStdDev = 1.0f;
    mMaskSize = 3;
    mIsModified = true;
    mRecreateMask = true;
    mDimensionCLCodeCompiledFor = 0;
}

GaussianSmoothingFilter::~GaussianSmoothingFilter() {
    delete[] mMask;
}

// TODO have to set mRecreateMask to true if input change dimension
void GaussianSmoothingFilter::createMask(Image::pointer input) {
    if(!mRecreateMask)
        return;

    unsigned char halfSize = (mMaskSize-1)/2;
    float sum = 0.0f;

    if(input->getDimensions() == 2) {
        mMask = new float[mMaskSize*mMaskSize];

        for(int x = -halfSize; x <= halfSize; x++) {
        for(int y = -halfSize; y <= halfSize; y++) {
            float value = exp(-(float)(x*x+y*y)/(2.0f*mStdDev*mStdDev));
            mMask[x+halfSize+(y+halfSize)*mMaskSize] = value;
            sum += value;
        }}

        for(int i = 0; i < mMaskSize*mMaskSize; i++)
            mMask[i] /= sum;
    } else if(input->getDimensions() == 3) {
         mMask = new float[mMaskSize*mMaskSize*mMaskSize];

        for(int x = -halfSize; x <= halfSize; x++) {
        for(int y = -halfSize; y <= halfSize; y++) {
        for(int z = -halfSize; z <= halfSize; z++) {
            float value = exp(-(float)(x*x+y*y+z*z)/(2.0f*mStdDev*mStdDev));
            mMask[x+halfSize+(y+halfSize)*mMaskSize+(z+halfSize)*mMaskSize*mMaskSize] = value;
            sum += value;
        }}}

        for(int i = 0; i < mMaskSize*mMaskSize*mMaskSize; i++)
            mMask[i] /= sum;
    }

    if(!mDevice->isHost()) {
        OpenCLDevice::pointer device = boost::static_pointer_cast<OpenCLDevice>(mDevice);
        unsigned int bufferSize = input->getDimensions() == 2 ? mMaskSize*mMaskSize : mMaskSize*mMaskSize*mMaskSize;
        mCLMask = cl::Buffer(
                device->getContext(),
                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                sizeof(float)*bufferSize,
                mMask
        );
    }

    mRecreateMask = false;
}

void GaussianSmoothingFilter::recompileOpenCLCode(Image::pointer input) {
    // Check if there is a need to recompile OpenCL code
    if(input->getDimensions() == mDimensionCLCodeCompiledFor &&
            input->getDataType() == mTypeCLCodeCompiledFor)
        return;

    OpenCLDevice::pointer device = boost::static_pointer_cast<OpenCLDevice>(mDevice);
    std::string buildOptions = "";
    if(input->getDataType() == TYPE_FLOAT) {
        buildOptions = "-DTYPE_FLOAT";
    } else if(input->getDataType() == TYPE_INT8 || input->getDataType() == TYPE_INT16) {
        buildOptions = "-DTYPE_INT";
    } else {
        buildOptions = "-DTYPE_UINT";
    }
    std::string filename;
    if(input->getDimensions() == 2) {
        filename = "Algorithms/GaussianSmoothingFilter2D.cl";
    } else {
        filename = "Algorithms/GaussianSmoothingFilter3D.cl";
    }
    int programNr = device->createProgramFromSource(std::string(FAST_ROOT_DIR) + filename, buildOptions);
    mKernel = cl::Kernel(device->getProgram(programNr), "gaussianSmoothing");
    mDimensionCLCodeCompiledFor = input->getDimensions();
    mTypeCLCodeCompiledFor = input->getDataType();
}

template <class T>
void executeAlgorithmOnHost(Image::pointer input, Image::pointer output, float * mask, unsigned char maskSize) {
    unsigned int nrOfComponents = input->getNrOfComponents();
    if(nrOfComponents != 1)
        throw Exception("Running the gaussian smoothing filter on an image with more than 1 component on the host is currently not supported.");
    ImageAccess inputAccess = input->getImageAccess(ACCESS_READ);
    ImageAccess outputAccess = output->getImageAccess(ACCESS_READ_WRITE);

    T * inputData = (T*)inputAccess.get();
    T * outputData = (T*)outputAccess.get();

    const unsigned char halfSize = (maskSize-1)/2;
    unsigned int width = input->getWidth();
    unsigned int height = input->getHeight();
    if(input->getDimensions() == 3) {
        unsigned int depth = input->getDepth();
        for(unsigned int z = halfSize; z < depth-halfSize; z++) {
        for(unsigned int y = halfSize; y < height-halfSize; y++) {
        for(unsigned int x = halfSize; x < width-halfSize; x++) {

            double sum = 0.0;
            for(int c = -halfSize; c <= halfSize; c++) {
            for(int b = -halfSize; b <= halfSize; b++) {
            for(int a = -halfSize; a <= halfSize; a++) {
                sum += mask[a+halfSize+(b+halfSize)*maskSize+(c+halfSize)*maskSize*maskSize]*
                        inputData[x+a+(y+b)*width+(z+c)*width*height];
            }}}
            outputData[x+y*width+z*width*height] = (T)sum;
        }}}
    } else {
        for(unsigned int y = halfSize; y < height-halfSize; y++) {
        for(unsigned int x = halfSize; x < width-halfSize; x++) {

            double sum = 0.0;
            for(int b = -halfSize; b <= halfSize; b++) {
            for(int a = -halfSize; a <= halfSize; a++) {
                sum += mask[a+halfSize+(b+halfSize)*maskSize]*
                        inputData[x+a+(y+b)*width];
            }}
            outputData[x+y*width] = (T)sum;
        }}
    }
}

void GaussianSmoothingFilter::execute() {

    Image::pointer input;
    if(!mInput.isValid()) {
        throw Exception("No input supplied to GaussianSmoothingFilter");
    }
    if(!mOutput.lock().isValid()) {
        // output object is no longer valid
        return;
    }
    if(mInput->isDynamicData()) {
        input = DynamicImage::pointer(mInput)->getNextFrame();
    } else {
        input = mInput;
    }

    Image::pointer output;
    if(mInput->isDynamicData()) {
        output = Image::New();
        DynamicImage::pointer(mOutput)->addFrame(output);
    } else {
        output = Image::pointer(mOutput);
    }

    // Initialize output image
    if(input->getDimensions() == 2) {
        output->create2DImage(input->getWidth(),
            input->getHeight(),
            input->getDataType(),
            input->getNrOfComponents(),
            mDevice);
    } else {
         output->create3DImage(input->getWidth(),
            input->getHeight(),
            input->getDepth(),
            input->getDataType(),
            input->getNrOfComponents(),
            mDevice);
    }


    createMask(input);

    if(mDevice->isHost()) {
        switch(input->getDataType()) {
            fastSwitchTypeMacro(executeAlgorithmOnHost<FAST_TYPE>(input, output, mMask, mMaskSize));
        }
    } else {
        OpenCLDevice::pointer device = boost::static_pointer_cast<OpenCLDevice>(mDevice);

        recompileOpenCLCode(input);
        cl::NDRange globalSize;
        if(input->getDimensions() == 2) {
            globalSize = cl::NDRange(input->getWidth(),input->getHeight());

            OpenCLImageAccess2D inputAccess = input->getOpenCLImageAccess2D(ACCESS_READ, device);
            OpenCLImageAccess2D outputAccess = output->getOpenCLImageAccess2D(ACCESS_READ_WRITE, device);
            mKernel.setArg(0, *inputAccess.get());
            mKernel.setArg(2, *outputAccess.get());
        } else {
            globalSize = cl::NDRange(input->getWidth(),input->getHeight(),input->getDepth());

            OpenCLImageAccess3D inputAccess = input->getOpenCLImageAccess3D(ACCESS_READ, device);
            OpenCLImageAccess3D outputAccess = output->getOpenCLImageAccess3D(ACCESS_READ_WRITE, device);
            mKernel.setArg(0, *inputAccess.get());
            mKernel.setArg(2, *outputAccess.get());
        }

        mKernel.setArg(1, mCLMask);
        mKernel.setArg(3, mMaskSize);

        device->getCommandQueue().enqueueNDRangeKernel(
                mKernel,
                cl::NullRange,
                globalSize,
                cl::NullRange
        );
    }

    if(!mInput->isDynamicData())
        mInput->release(mDevice);

    // Update the timestamp of the output data
    output->updateModifiedTimestamp();
}

void GaussianSmoothingFilter::waitToFinish() {
    if(!mDevice->isHost()) {
        OpenCLDevice::pointer device = boost::static_pointer_cast<OpenCLDevice>(mDevice);
        device->getCommandQueue().finish();
    }
}

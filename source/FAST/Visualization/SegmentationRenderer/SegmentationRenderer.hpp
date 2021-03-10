#pragma once

#include "FAST/Visualization/ImageRenderer/ImageRenderer.hpp"
#include "FAST/Data/Segmentation.hpp"
#include "FAST/Data/Color.hpp"
#include "FAST/Utility.hpp"
#include <unordered_map>
#include <mutex>
#include <FAST/Visualization/LabelColorRenderer.hpp>

namespace fast {

class FAST_EXPORT  SegmentationRenderer : public ImageRenderer, public LabelColorRenderer {
    FAST_OBJECT(SegmentationRenderer)
    public:
        void setColor(Segmentation::LabelType, Color);
        void setColor(int label, Color);
        void setFillArea(Segmentation::LabelType, bool);
        void setFillArea(bool fillArea);
        void setBorderRadius(int radius);
        void setOpacity(float opacity);
        void setInterpolation(bool useInterpolation);
        void loadAttributes() override;
    private:
        SegmentationRenderer();
        void draw(Matrix4f perspectiveMatrix, Matrix4f viewingMatrix, float zNear, float zFar, bool mode2D) override;

        bool mColorsModified;
        bool mFillAreaModified;

        std::unordered_map<int, Color> mLabelColors;
        std::unordered_map<int, bool> mLabelFillArea;
        bool mFillArea;
        bool mUseInterpolation = true;
        int mBorderRadius = 1;
        float mOpacity = 1;
        cl::Buffer mColorBuffer, mFillAreaBuffer;
};

} // end namespace fast


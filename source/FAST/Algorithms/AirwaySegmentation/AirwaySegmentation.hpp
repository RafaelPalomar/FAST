#ifndef AIRWAY_SEGMENTATION_HPP_
#define AIRWAY_SEGMENTATION_HPP_

#include "FAST/Algorithms/SegmentationAlgorithm.hpp"

namespace fast {

class Image;
class Segmentation;

class FAST_EXPORT  AirwaySegmentation : public SegmentationAlgorithm {
	FAST_OBJECT(AirwaySegmentation)
	public:
	    void setSeedPoint(int x, int y, int z);
		void setSeedPoint(Vector3i seed);
		/**
		 * Set the sigma value of the gaussian smoothing performed before segmentation.
		 * Default is 0.5. A higher value can be used for low dose CT.
		 * @param sigma
		 */
		void setSmoothing(float sigma);
		void loadAttributes() override;
	private:
		AirwaySegmentation();
		void execute();
		static Vector3i findSeedVoxel(std::shared_ptr<Image> volume);
		std::shared_ptr<Image> convertToHU(std::shared_ptr<Image> image);
		void morphologicalClosing(std::shared_ptr<Segmentation> segmentation);

		Vector3i mSeedPoint;
		float mSmoothingSigma = 0.5;
        bool mUseManualSeedPoint = false;
};

}


#endif

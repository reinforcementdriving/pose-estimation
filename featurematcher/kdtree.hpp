#pragma once

#include <pcl/correspondence.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/kdtree/impl/kdtree_flann.hpp>

#include "../featurematching.hpp"
#include "../types.h"
#include "../parameter.h"
#include "../pointcloud.h"

namespace PoseEstimation
{
    /**
     * @brief Feature matching using Kd-Trees.
     */
    template<typename DescriptorT>
    class KdTreeFeatureMatcher : public FeatureMatcher<DescriptorT>
    {
    public:
        KdTreeFeatureMatcher() : FeatureMatcher<DescriptorT>()
        {
        }

        virtual void match(const typename pcl::PointCloud<DescriptorT>::Ptr &source_descriptors,
                           const typename pcl::PointCloud<DescriptorT>::Ptr &target_descriptors,
                           pcl::CorrespondencesPtr &correspondences)
        {
            typename pcl::KdTreeFLANN<DescriptorT> match_search;
            match_search.setInputCloud(target_descriptors);

            double sum = 0, number = 0; // used for avg calculation
            typename pcl::DefaultPointRepresentation<DescriptorT> representation;
            size_t skipped = 0;
            for (size_t i = 0; i < source_descriptors->size(); ++i)
            {
                if (!representation.isValid(source_descriptors->at(i))) // skip NaNs
                {
                    ++skipped;
                    continue;
                }

                // find the single closest descriptor
                std::vector<int> nn_idx(1);     // indices of nearest neighbors
                std::vector<float> nn_dist(1);  // distances of nearest neighbors
                int found_neighs = match_search.nearestKSearch(source_descriptors->at(i), 1, nn_idx, nn_dist);
                for (size_t j = 0; j < found_neighs; ++j)
                {
                    pcl::Correspondence corr(static_cast<int>(i), nn_idx[j], nn_dist[j]);
                    correspondences->push_back(corr);

                    sum += nn_dist[j];
                }

                number += found_neighs;

                nn_idx.clear();
                nn_dist.clear();
            }

            if (skipped > 0)
            {
                Logger::warning(boost::format("Skipped %d of %d source cloud descriptors because they were invalid.")
                                % skipped % source_descriptors->size());
            }

            if (correspondences->empty())
            {
                Logger::error("No correspondences between source and target descriptors were found.");
            }

            std::sort(correspondences->begin(), correspondences->end(), _compareCorrespondences);

            // remove correspondences that are not within the given percentage
            const size_t total = correspondences->size();
            const size_t top = (size_t)(matchThreshold.value<float>() * correspondences->size());
            correspondences->erase(correspondences->begin() + top, correspondences->end());

            Logger::debug(boost::format("Found %i correspondences in the top %.2f%% closest distance range out of %i total correspondences.")
                          % correspondences->size() % (matchThreshold.value<float>() * 100.0f) % total);
            Logger::debug(boost::format("Average correspondence distance: %d") % (sum/number));
        }

        static ParameterCategory argumentCategory;
        PARAMETER_CATEGORY_GETTER(argumentCategory)

        static Parameter matchThreshold;

    private:
        static bool _compareCorrespondences(const pcl::Correspondence &l, const pcl::Correspondence &r)
        {
            return l.distance < r.distance;
        }
    };

    template<typename DescriptorT>
    ParameterCategory KdTreeFeatureMatcher<DescriptorT>::argumentCategory(
                "kdmatch", "Feature matching using Kd-Trees",
                PipelineModuleType::FeatureMatcher);

    template<typename DescriptorT>
    Parameter KdTreeFeatureMatcher<DescriptorT>::matchThreshold = Parameter(
                "kdmatch",
                "thresh",
                (float)1.0f,
                "Top percentage of correspondence distances that are considered",
                NUMERICAL_PARAMETER_RANGE(0.1, 1.0));
}

## #############################################################################
## Subdirs exe directories
## #############################################################################

add_subdirectory(build_samples)
add_subdirectory(gamma_estimation)

if(BUILD_TESTING)
  add_subdirectory(gamma_estimation_test)
endif()

add_subdirectory(local_patch_covariance_distance)
add_subdirectory(local_patch_mean_distance)
add_subdirectory(low_memory_tools)
add_subdirectory(roi_intensities_stats)

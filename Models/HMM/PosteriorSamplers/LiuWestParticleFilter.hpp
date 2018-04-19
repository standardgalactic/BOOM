#ifndef BOOM_HMM_LIU_WEST_PARTICLE_FILTER_HPP_
#define BOOM_HMM_LIU_WEST_PARTICLE_FILTER_HPP_
/*
  Copyright (C) 2005-2018 Steven L. Scott

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License along
  with this library; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include "Models/HMM/GeneralHmm.hpp"
#include <cstdint>

namespace BOOM {

  
  // A particle filter for learning model parameters and state simultaneously.
  //
  // At each update a kernel density estimate of the model parameters is used to
  // simulate new parameter values.  The KDE is an alternative to the empirical
  // distribution, and helps prevent particle collapse.
  class LiuWestParticleFilter {
   public:
    // Args:
    //   hmm:  The general HMM to be filtered.
    //   number_of_particles:  The desired number of particles.
    //   kernel_scale_factor: The kernel density estimate of the parameters has
    //     variance = kernel_scale_factor^2 *
    //     sample_variance(parameter_particles).
    //
    // TODO:  Better default values for kernel scale factor.
    LiuWestParticleFilter(const Ptr<GeneralContinuousStateHmm> &hmm,
                          int number_of_particles,
                          double kernel_scale_factor = .01);

    // Update the particle distribution with new information.
    //
    // Args:
    //   rng:  The random number generator to use for the update.
    //   observation:  A new data point.
    //   observation_time: The time index ('t') when the observation was
    //     observed.
    void update(RNG &rng, const Data &observation, int observation_time);

    int64_t number_of_particles() const {return state_particles_.size();}
        
   private:
    Ptr<GeneralContinuousStateHmm> hmm_;
    std::vector<Vector> state_particles_;
    std::vector<Vector> parameter_particles_;
    Vector log_weights_;
    double kernel_scale_factor_;
  };
  
}


#endif  // BOOM_HMM_LIU_WEST_PARTICLE_FILTER_HPP_

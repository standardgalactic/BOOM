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

#include "Models/HMM/PosteriorSamplers/LiuWestParticleFilter.hpp"
#include "distributions.hpp"
#include "Models/MvnBase.hpp"

namespace BOOM {

  LiuWestParticleFilter::LiuWestParticleFilter(
      const Ptr<GeneralContinuousStateHmm> &hmm,
      int number_of_particles,
      double kernel_scale_factor)
      : hmm_(hmm),
        log_weights_(number_of_particles),
        kernel_scale_factor_(kernel_scale_factor)
  {
    if (number_of_particles <= 0) {
      report_error("The number of particles must be positive.");
    }

    if (kernel_scale_factor_ <= 0 || kernel_scale_factor_ >= 1.0) {
      report_error("Kernel scale factor parameter must be strictly between "
                   "0 and 1.");
    }

    state_particles_.reserve(number_of_particles);
    parameter_particles_.reserve(number_of_particles);

    Vector parameter_vector = hmm_->vectorize_params(true);
    for (int i = 0; i < number_of_particles; ++i) {
      state_particles_.push_back(Vector(hmm_->state_dimension()));
      parameter_particles_.push_back(parameter_vector);
    }
    /////////////////////////////////////////////////////////
    // TODO:
    //   1) Initialize particles.
    /////////////////////////////////////////////////////////
  }

  void LiuWestParticleFilter::update(RNG &rng,
                                     const Data &observation,
                                     int observation_time) {
    cout << "Calling update at time " << observation_time << endl;

    //====== Step 1
    // Compute the means and variances to be used in the kernel density
    // estimate.
    std::vector<Vector> predicted_state_mean;
    predicted_state_mean.reserve(number_of_particles());
    MvnSuf suf(parameter_particles_[0].size());
    for (int i = 0; i < number_of_particles(); ++i) {
      suf.update_raw(parameter_particles_[i]);
      predicted_state_mean.push_back(hmm_->predicted_state_mean(
          state_particles_[i], observation_time, parameter_particles_[i]));
    }
    Vector parameter_mean = suf.ybar();
    Vector kernel_weights(number_of_particles());
    double max_log_weight = negative_infinity();
    std::vector<Vector> predicted_parameter_mean(number_of_particles());
    double particle_weight = sqrt(1 - square(kernel_scale_factor_));
    for (int i = 0; i < number_of_particles(); ++i) {
      predicted_parameter_mean[i] = 
          particle_weight * parameter_particles_[i]
          + (1 - particle_weight) * parameter_mean;
      kernel_weights[i] = log_weights_[i] + hmm_->log_observation_density(
          observation,
          predicted_state_mean[i],
          observation_time,
          predicted_parameter_mean[i]);
      max_log_weight = std::max<double>(max_log_weight, kernel_weights[i]);
    }
    double total_weight = 0;
    for (int i = 0; i < number_of_particles(); ++i) {
      double weight = exp(kernel_weights[i] - max_log_weight);
      total_weight += weight;
      kernel_weights[i] = weight;
    }
    kernel_weights /= total_weight;

    SpdMatrix sample_variance = suf.sample_var();
    if (sample_variance.rank() < sample_variance.nrow()) {
      ////////////////
      // Refresh the parameter distribution.
    }
    Chol sample_variance_cholesky(sample_variance);
    if (!sample_variance_cholesky.is_pos_def()) {
      report_error("Sample variance is not positive definite.");
    }
    Matrix variance_cholesky =
        kernel_scale_factor_ * sample_variance_cholesky.getL();

    //===== Step 2:
    // Propose new values of state and parameters, and update the weights.
    //
    // Space is needed for the new proposals, because sampling and updating is
    // done with replacement.
    std::vector<Vector> new_state_particles(number_of_particles());
    Vector new_log_weights(number_of_particles());
    for (int i = 0; i < number_of_particles(); ++i) {
      int particle = rmulti_mt(rng, kernel_weights);
      Vector parameter_proposal =
          rmvn_L_mt(rng, predicted_parameter_mean[particle], variance_cholesky);
      Vector state_proposal = hmm_->simulate_transition(
          rng, state_particles_[particle], observation_time - 1, parameter_proposal);
      parameter_particles_[i] = parameter_proposal;
      new_state_particles[i] = state_proposal;
      new_log_weights[i] =
          hmm_->log_observation_density(observation,
                                        state_proposal,
                                        observation_time,
                                        parameter_proposal)
          - hmm_->log_observation_density(observation,
                                          predicted_state_mean[particle],
                                          observation_time,
                                          predicted_parameter_mean[particle]);
    }
    std::swap(new_state_particles, state_particles_);
    std::swap(new_log_weights, log_weights_);
  }  
}  // namespace BOOM

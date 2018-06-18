#ifndef BOOM_STATE_SPACE_KALMAN_FILTER_BASE_HPP_
#define BOOM_STATE_SPACE_KALMAN_FILTER_BASE_HPP_

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

#include "LinAlg/SpdMatrix.hpp"
#include "LinAlg/Vector.hpp"
#include "LinAlg/VectorView.hpp"
#include "LinAlg/Selector.hpp"

namespace BOOM {
  class StateSpaceModelBase;
  namespace Kalman {
    //---------------------------------------------------------------------------
    // The Kalman filter sequentially updates a set of marginal distributions
    // conditional on prior data.  The marginal distributions describe the
    // latent state variables, as well as the error terms of the model and the
    // forecast distribution of the next observation.
    //
    // Several base classes are needed for marginal distributions to account for
    // the fact that univariate and multivariate data must be handled
    // differently, and that there are several potentially simplifying
    // assumptions in the multivariate case.

    // A base class to handle quantities common to all marginal distributions.
    class MarginalDistributionBase {
     public:
      MarginalDistributionBase(int dim);
      virtual ~MarginalDistributionBase() {}
      
      const Vector &state_mean() const {return state_mean_;}
      void set_state_mean(const Vector &state_mean) {state_mean_ = state_mean;}
      void increment_state_mean(const Vector &v) { state_mean_ += v; }
      const SpdMatrix &state_variance() const {return state_variance_;}
      void set_state_variance(const SpdMatrix &var) { state_variance_ = var;}
      void increment_state_variance(const SpdMatrix &m) {state_variance_ += m;}

     protected:
      SpdMatrix & mutable_state_variance() {return state_variance_;}
      
     private:
      Vector state_mean_;
      SpdMatrix state_variance_;
    };

  }  // namespace Kalman

  // A base class for Kalman filter objects.  This class keeps track of the log
  // likelihood, the status of the filter, and takes responsibility for setting
  // observers on parameters and data.
  class KalmanFilterBase {
   public:
    KalmanFilterBase();
    virtual ~KalmanFilterBase() {}
    
    // The status of the Kalman filter.
    // Values:
    //   NOT_CURRENT: The filter must be re-run before its entries can be used.
    //   MCMC_CURRENT: neither parameter nor data have changed since
    //     impute_state() was last called.  state posterior means and variances
    //     are not available.
    //   CURRENT: Neither parameters nor data have changed since
    //     full_kalman_filter() was last called.
    enum KalmanFilterStatus { NOT_CURRENT, MCMC_CURRENT, CURRENT };

    double log_likelihood() const {
      return log_likelihood_;
    }

    double compute_log_likelihood() {
      if (status_ == NOT_CURRENT) {
        clear();
        update();
      }
      return log_likelihood_;
    }

    // Set log likelihood to zero and status to NOT_CURRENT.
    void clear();

    // Run the kalman filter over all the data contained in *model_.
    virtual void update() = 0;

    // Run the Durbin and Koopman fast disturbance smoother.
    virtual Vector fast_disturbance_smooth() = 0;

    void mark_not_current() {
      status_ = NOT_CURRENT;
    }

    virtual Kalman::MarginalDistributionBase & operator[](size_t pos) = 0;
    virtual const Kalman::MarginalDistributionBase & operator[](
        size_t pos) const = 0;

    void set_status(const KalmanFilterStatus &status) {
      status_ = status;
    }
    
   protected:
    void increment_log_likelihood(double loglike) {
      log_likelihood_ += loglike;
    }

    void observe_model_parameters(StateSpaceModelBase *model);

   private:
    KalmanFilterStatus status_;
    double log_likelihood_;
  };
  
}  // namespace BOOM

#endif  // BOOM_STATE_SPACE_KALMAN_FILTER_BASE_HPP_
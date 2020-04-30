#ifndef BOOM_GLM_LOGLINEAR_MODEL_HPP_
#define BOOM_GLM_LOGLINEAR_MODEL_HPP_
/*
  Copyright (C) 2005-2020 Steven L. Scott

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

#include <map>
#include <cstdint>

#include "Models/CategoricalData.hpp"
#include "Models/Sufstat.hpp"
#include "Models/Policies/ParamPolicy_1.hpp"
#include "Models/Policies/SufstatDataPolicy.hpp"
#include "Models/Policies/PriorPolicy.hpp"
#include "Models/Glm/GlmCoefs.hpp"
#include "Models/Glm/Encoders.hpp"

#include "LinAlg/Array.hpp"

#include "distributions/rng.hpp"
#include "stats/DataTable.hpp"

namespace BOOM {

  //===========================================================================
  // A data type representing a collection of categorical variables.
  class MultivariateCategoricalData
      : public Data {
   public:
    MultivariateCategoricalData() {}

    MultivariateCategoricalData(const MultivariateCategoricalData &rhs);
    MultivariateCategoricalData(MultivariateCategoricalData &&rhs) = default;
    MultivariateCategoricalData & operator=(const MultivariateCategoricalData &rhs);
    MultivariateCategoricalData &operator=(MultivariateCategoricalData &&rhs) = default;

    MultivariateCategoricalData *clone() const override {
      return new MultivariateCategoricalData(*this);
    }

    std::ostream &display(std::ostream &out) const override;

    // Add a new categorical variable to the back of the collection.
    void push_back(const Ptr<CategoricalData> &scalar) {
      data_.push_back(scalar);
    }

    // Recover the variable in position i.
    const CategoricalData & operator[](int i) const {
      return *data_[i];
    }

    // The number of variables in the collection.
    int nvars() const { return data_.size(); }

   private:
    std::vector<Ptr<CategoricalData>> data_;
  };

  //===========================================================================
  // Convert a categorical variable to a Vector suitable for analysis by a
  // LoglinearModel.
  class CategoricalDataEncoder : private RefCounted {
   public:

    // A vector containing a 1/0/-1 effects encoding of the input data.
    virtual Vector encode(const MultivariateCategoricalData &data) const = 0;
    Vector encode(const std::vector<int> &data) const;

    // The number of columns in the Vector returned by 'encode'.
    virtual int dim() const = 0;

    // The indices of the variables driving this effect.
    virtual const std::vector<int> &which_variables() const = 0;

    // The number of levels in each variable.
    virtual const std::vector<int> &nlevels() const = 0;

   private:
    friend void intrusive_ptr_add_ref(CategoricalDataEncoder *d) {
      d->up_count();
    }
    friend void intrusive_ptr_release(CategoricalDataEncoder *d) {
      d->down_count();
      if (d->ref_count() == 0) {
        delete d;
      }
    }
  };

  //---------------------------------------------------------------------------
  // A CategoricalDataEncoder focusing on a single variable.
  class CategoricalMainEffect : public CategoricalDataEncoder {
   public:
    CategoricalMainEffect(int which_variable, const Ptr<CatKeyBase> &key);

    Vector encode(const MultivariateCategoricalData &data) const override;
    Vector encode(const std::vector<int> &data) const;
    int dim() const override {return encoder_.dim();}
    const std::vector<int> &which_variables() const override {
      return which_variables_;
    }
    const std::vector<int> &nlevels() const override {return nlevels_;}

   private:
    EffectsEncoder encoder_;

    // Identifies the index of the relevant variable.
    std::vector<int> which_variables_;

    // The number of levels in the relevant variable.
    std::vector<int> nlevels_;
  };

  //---------------------------------------------------------------------------
  // A CategoricalDataEncoder representing the interaction between lower-order
  // effects.  Interactions are built from two effects at a time.  Higher order
  // interactions are built from the interaction of two lower order interactions
  // or main effects.
  class CategoricalInteraction :
      public CategoricalDataEncoder {
   public:
    CategoricalInteraction(const Ptr<CategoricalDataEncoder> &enc1,
                           const Ptr<CategoricalDataEncoder> &enc2);

    Vector encode(const MultivariateCategoricalData &data) const;
    Vector encode(const std::vector<int> &data) const;
    int dim() const override {return enc1_->dim() * enc2_->dim();}
    const std::vector<int> &which_variables() const override {
      return which_variables_;
    }
    const std::vector<int> &nlevels() const {return nlevels_;}

   private:
    Ptr<CategoricalDataEncoder> enc1_;
    Ptr<CategoricalDataEncoder> enc2_;
    std::vector<int> which_variables_;
    std::vector<int> nlevels_;
  };

  //---------------------------------------------------------------------------
  // The "parent" encoder class containing main effects and interactions.
  class MultivariateCategoricalEncoder {
   public:
    MultivariateCategoricalEncoder() : dim_(0) {}

    void add_effect(const Ptr<CategoricalDataEncoder> &effect);

    int dim() const {return dim_;}

    Vector encode(const MultivariateCategoricalData &data) const;
    Vector encode(const std::vector<int> &data) const;

   private:
    std::vector<Ptr<CategoricalDataEncoder>> encoders_;
    int dim_;
  };

  //===========================================================================
  // The sufficient statistics for a log linear model are the marginal cross
  // tabulations for each effect in the model.
  class LoglinearModelSuf : public SufstatDetails<MultivariateCategoricalData> {
   public:
    LoglinearModelSuf() : sample_size_(0), valid_(true) {}
    LoglinearModelSuf *clone() const override {
      return new LoglinearModelSuf(*this);
    }

    std::ostream &print(std::ostream &out) const override;

    // vectorize/unvectorize packs the data but not the sizes or model
    // structure.
    Vector vectorize(bool minimal = true) const override;
    Vector::const_iterator unvectorize(
        Vector::const_iterator &v, bool minimal=true) override;
    Vector::const_iterator unvectorize(
        const Vector &v, bool minimal=true) override;

    // Add a main effect or interaction to the model structure.
    //
    // If data has already been allocated to the object, adding an effect
    // invalidates the object.  To put it back in a valid state call "refresh"
    // and pass the original data.
    //
    // If all elements of model structure are added prior to calling
    // Update. Then no refreshing is needed.
    void add_effect(const Ptr<CategoricalDataEncoder> &effect);

    // Clear the data but keep the information about model structure.  Set the
    // valid_ flag to true.
    void clear() override;

    // Clear everything.
    void clear_data_and_structure();

    // Clear the data and recompute the sufficient statistics.
    void refresh(const std::vector<Ptr<MultivariateCategoricalData>> &data);

    // It is an error to update the sufficient statistics with new data when the
    // object is in an invalid state.  The easiest way to prevent this from
    // happening is to add all elements of model structure before calling
    // update.
    void Update(const MultivariateCategoricalData &data) override;

    // Note that 'combine' assumes that the two suf's being combined have the
    // same structure.
    void combine(const LoglinearModelSuf &suf);
    void combine(const Ptr<LoglinearModelSuf> &suf);
    LoglinearModelSuf *abstract_combine(Sufstat *s);

    // Args:
    //   index: The indices of the variables in the desired margin.  For main
    //     effects the index will just contain one number.  For 2-way
    //     interactions it will contain 2 numbers, and for k-way interactions it
    //     will contain k numbers.  The elements of 'index' should be in
    //     increasing order: [0, 3, 4] is okay.  [3, 0, 4] is not.
    //
    // Returns:
    //   An array with dimensions corresponding to the variables in the desired
    //   margin.  The index of each array dimension corresponds to the level of
    //   that variable.  The array entry at (for example) (i, j, k) is the
    //   number of times X0 == i, X1 == j, and X2 == k.
    const Array &margin(const std::vector<int> &index) const;

   private:
    std::vector<Ptr<CategoricalDataEncoder>> effects_;

    // Cross tabulations are indexed by a vector containing the indices of the
    // tabulated variables.  For example, a 3-way interaction might include
    // variables 0, 2, and 5.  The indices must be in order.
    std::map<std::vector<int>, Array> cross_tabulations_;

    std::int64_t sample_size_;

    // The state of the object.  The state becomes invalid each time an effect
    // is added.  The state can be made valid by calling clear() or refresh().
    bool valid_;
  };

  //===========================================================================
  class LoglinearModel
      : public ParamPolicy_1<GlmCoefs>,
        public SufstatDataPolicy<MultivariateCategoricalData, LoglinearModelSuf>,
        public PriorPolicy {
   public:

    // An empty LoglinearModel.  The fisrt time this model calls add_data main
    // effects will be added for each variable in the added data point.
    // If interactions are later added, then
    LoglinearModel();

    // Build a LoglinearModel from the categorical variables in a DataTable.
    //
    // A model built with this constructor must call refresh_suf() after all
    // model structure is added.
    explicit LoglinearModel(const DataTable &table);

    LoglinearModel *clone() const override;

    void add_data(const Ptr<MultivariateCategoricalData> &data_point) override;
    void add_data(const Ptr<Data> &dp) { add_data(DAT(dp)); }
    void add_data(MultivariateCategoricalData *dp) {
      add_data(Ptr<MultivariateCategoricalData>(dp));
    }

    // Perform one Gibbs sampling pass over the data point.
    void impute(MultivariateCategoricalData &data_point, RNG &rng);

    // The number of categorical variables being modeled.
    int nvars() const;

    void add_interaction(const std::vector<int> &variable_postiions);

    void refresh_suf();

    const GlmCoefs &coef() const {return prm_ref();}

    double logp(const MultivariateCategoricalData &data_point) const;
    double logp(const std::vector<int> &data_point) const;

   private:
    // Add the effect to the encoder, to the sufficient statistics, and resize
    // the coefficient vector.
    void add_effect(const Ptr<CategoricalDataEncoder> &effect);

    // The main_effects are used to build interaction terms.
    std::vector<Ptr<CategoricalMainEffect>> main_effects_;

    MultivariateCategoricalEncoder encoder_;
  };

}  // namespace BOOM

#endif  // BOOM_GLM_LOGLINEAR_MODEL_HPP_

/*
 * (C) Copyright 2018-2019 UCAR
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#ifndef UFO_FILTERS_OBSBOUNDSCHECK_H_
#define UFO_FILTERS_OBSBOUNDSCHECK_H_

#include <ostream>
#include <string>

#include "boost/shared_ptr.hpp"

#include "eckit/config/LocalConfiguration.h"
#include "ioda/ObsDataVector.h"
#include "ioda/ObsSpace.h"
#include "oops/base/Variables.h"
#include "oops/util/ObjectCounter.h"
#include "oops/util/Printable.h"
#include "ufo/filters/ObsFilterData.h"
#include "ufo/filters/Variables.h"

namespace ioda {
  template <typename DATATYPE> class ObsDataVector;
  class ObsVector;
}

namespace ufo {
  class GeoVaLs;
  class ObsDiagnostics;

/// ObsBoundsCheck: generic quality control based on observation data only

// Check that observations are within some bounds over some domain

class ObsBoundsCheck : public util::Printable,
                       private util::ObjectCounter<ObsBoundsCheck> {
 public:
  static const std::string classname() {return "ufo::ObsBoundsCheck";}

  ObsBoundsCheck(ioda::ObsSpace &, const eckit::Configuration &,
                 boost::shared_ptr<ioda::ObsDataVector<int> >,
                 boost::shared_ptr<ioda::ObsDataVector<float> >);
  ~ObsBoundsCheck();

  void preProcess() const {}
  void priorFilter(const GeoVaLs &);
  void postFilter(const ioda::ObsVector &, const ObsDiagnostics &);

  const oops::Variables & requiredGeoVaLs() const {return geovars_;}
  const oops::Variables & requiredHdiagnostics() const {return diagvars_;}

 private:
  void print(std::ostream &) const;

  ioda::ObsSpace & obsdb_;
  ObsFilterData data_;
  const eckit::LocalConfiguration config_;
  ufo::Variables allvars_;
  oops::Variables geovars_;
  oops::Variables diagvars_;
  ioda::ObsDataVector<int> & flags_;
};

}  // namespace ufo

#endif  // UFO_FILTERS_OBSBOUNDSCHECK_H_

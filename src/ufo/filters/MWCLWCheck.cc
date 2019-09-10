/*
 * (C) Copyright 2018-2019 UCAR
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#include "ufo/filters/MWCLWCheck.h"

#include <math.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "eckit/config/Configuration.h"

#include "ioda/ObsDataVector.h"
#include "ioda/ObsSpace.h"
#include "oops/interface/ObsFilter.h"
#include "oops/util/Logger.h"
#include "oops/util/missingValues.h"
#include "ufo/filters/processWhere.h"
#include "ufo/filters/QCflags.h"
#include "ufo/UfoTrait.h"

namespace ufo {

// -----------------------------------------------------------------------------

MWCLWCheck::MWCLWCheck(ioda::ObsSpace & obsdb, const eckit::Configuration & config,
                               boost::shared_ptr<ioda::ObsDataVector<int> > flags,
                               boost::shared_ptr<ioda::ObsDataVector<float> >)
  : obsdb_(obsdb), data_(obsdb_), config_(config),
    allvars_(getAllWhereVariables(config_)), geovars_(allvars_.allFromGroup("GeoVaLs")),
    diagvars_(allvars_.allFromGroup("ObsDiag")), flags_(*flags)
{
  oops::Log::debug() << "MWCLWCheck: config = " << config_ << std::endl;
  oops::Log::debug() << "MWCLWCheck: geovars = " << geovars_ << std::endl;
}

// -----------------------------------------------------------------------------

MWCLWCheck::~MWCLWCheck() {}

// -----------------------------------------------------------------------------

void MWCLWCheck::priorFilter(const GeoVaLs & gv) {
  data_.associate(gv);
}

// -----------------------------------------------------------------------------

void MWCLWCheck:: postFilter(const ioda::ObsVector & hofx, const ObsDiagnostics &) {
  oops::Log::trace() << "MWCLWCheck postFilter" << std::endl;
  data_.associate(hofx);

  oops::Variables vars(config_);
  oops::Variables observed = obsdb_.obsvariables();

  eckit::LocalConfiguration inconf(config_, "inputs");
  oops::Variables invars(inconf);

  const float missing = util::missingValue(missing);
  float amsua_clw(float, float, float);

// Get config
  std::vector<float> clw_thresholds = config_.getFloatVector("clw_thresholds");
// clw_option controls how the clw is calculated:
//     1) Use observed BTs
//     2) Use calculated BTs
//     3) Symmetric calculation
  const int clw_option = config_.getInt("clw_option", missing);
  std::cout << "MWCLWCheck: config = " << config_ << std::endl;

// Number of channels to be tested and number of thresholds needs to be the same
  ASSERT(clw_thresholds.size() == vars.size());
// Check we have the correct number of channels to do the CLW calculation
  ASSERT(invars.size() == 2);
// Check clw_option is in range
  ASSERT(clw_option >= 1 && clw_option <=3);

// Select where the CLW check will apply
  std::vector<bool> apply = processWhere(config_, data_);

  ioda::ObsDataVector<float> obs(obsdb_, vars, "ObsValue");
  ioda::ObsDataVector<float> obs_for_calc(obsdb_, invars, "ObsValue");
  ioda::ObsDataVector<float> sza(obsdb_, "sensor_zenith_angle", "MetaData");
  ioda::ObsDataVector<float> clw(obsdb_, "cloud_liquid_water", "Diagnostic", false);
  ioda::ObsDataVector<float> clw_guess_out(obsdb_, "clws_guess", "Diagnostic", false);
  ioda::ObsDataVector<float> clw_obs_out(obsdb_, "clw_obs", "Diagnostic", false);

// Loop over obs locations calculating CLW from observations
  float clw_obs = missing, clw_guess = missing;
  for (size_t jobs = 0; jobs < obs.nlocs(); ++jobs) {
      switch (clw_option) {
           case 1 :
               clw_obs = amsua_clw(obs_for_calc[0][jobs], obs_for_calc[1][jobs],
                               sza[0][jobs]);
               clw[0][jobs] = clw_obs;
               break;
           case 2 :
               clw_guess = amsua_clw(hofx[observed.size() * jobs + observed.find(invars[0])],
                                     hofx[observed.size() * jobs + observed.find(invars[1])],
                                     sza[0][jobs]);
               clw[0][jobs] = clw_guess;
               break;
           case 3 :
               clw_obs = amsua_clw(obs_for_calc[0][jobs], obs_for_calc[1][jobs], sza[0][jobs]);
               clw_guess = amsua_clw(hofx[observed.size() * jobs + observed.find(invars[0])],
                                     hofx[observed.size() * jobs + observed.find(invars[1])],
                                     sza[0][jobs]);
               clw_obs_out[0][jobs] = clw_obs;
               clw_guess_out[0][jobs] = clw_guess;
               if (clw_obs != missing && clw_guess != missing)
               {
                 clw[0][jobs] = (clw_obs + clw_guess)/2.0;
               } else {
                 clw[0][jobs] = missing;
               }
               break;
      }

// Apply CLW threshold to observations
     for (size_t jv = 0; jv < vars.size(); ++jv) {
        size_t iv = observed.find(vars[jv]);
        if (apply[jobs] && flags_[iv][jobs] == 0) {
           if (clw_thresholds[jv] != missing && (clw[0][jobs] == missing ||
               clw[0][jobs] > clw_thresholds[jv])) flags_[iv][jobs] = QCflags::clw;
      }
    }
  }
  clw.save("Derived");
  clw_obs_out.save("Derived");
  clw_guess_out.save("Derived");
}
// -----------------------------------------------------------------------------

float amsua_clw(float tobs1, float tobs2, float sza)
{
    const float d1 = 0.754;
    const float d2 = -2.265;
    const float missing = util::missingValue(missing);

    float clw;
    float cossza = cos(M_PI * sza/180.0);
    float d0 = 8.240 - (2.622 - 1.846*cossza)*cossza;
    if (tobs1 <= 284.0 && tobs2 <= 284.0 && tobs1 > 0.0 && tobs2 > 0.0)
    {
        clw = cossza*(d0 + d1*log(285.0-tobs1)) + d2*log(285.0-tobs2);
        clw = std::max(0.0f, clw);
    } else {
        clw = missing;
    }

    return clw;
}

// -----------------------------------------------------------------------------

void MWCLWCheck::print(std::ostream & os) const {
  os << "MWCLWCheck: config = " << config_ << std::endl;
}

// -----------------------------------------------------------------------------

}  // namespace ufo

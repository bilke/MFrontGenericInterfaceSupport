/*!
 * \file   src/Behaviour.cxx
 * \brief
 * \author Thomas Helfer
 * \date   19/06/2018
 * \copyright (C) Copyright Thomas Helfer 2018.
 * Use, modification and distribution are subject
 * to one of the following licences:
 * - GNU Lesser General Public License (LGPL), Version 3.0. (See accompanying
 *   file LGPL-3.0.txt)
 * - CECILL-C,  Version 1.0 (See accompanying files
 *   CeCILL-C_V1-en.txt and CeCILL-C_V1-fr.txt).
 */

#include <cstdlib>
#include <iterator>

#include "MGIS/Raise.hxx"
#include "MGIS/LibrariesManager.hxx"
#include "MGIS/Behaviour/Hypothesis.hxx"
#include "MGIS/Behaviour/Behaviour.hxx"

namespace mgis::behaviour {

  static std::array<mgis::real, 9u> buildRotationMatrix(const real *const a) {
    return buildRotationMatrix(mgis::span<const mgis::real, 2u>{a, 2u});
  }  // end of buildRotationMatrix

  static std::array<mgis::real, 9u> buildRotationMatrix(const real *const a1,
                                                        const real *const a2) {
    return buildRotationMatrix(mgis::span<const mgis::real, 3u>{a1, 3u},
                               mgis::span<const mgis::real, 3u>{a2, 3u});
  }  // end of buildRotationMatrix

  template <typename ErrorHandler>
  static std::vector<Variable> buildVariablesList(
      ErrorHandler &raise,
      const std::vector<std::string> &names,
      const std::vector<int> &types) {
    std::vector<Variable> vars;
    if (names.size() != types.size()) {
      raise(
          "the number of internal state variables names does not match "
          "the number of internal state variables types");
    }
    for (decltype(names.size()) i = 0; i != names.size(); ++i) {
      vars.push_back({names[i], getVariableType(types[i]), types[i]});
    }
    return vars;
  }  // end of buildVariablesList

  template <typename ErrorHandler>
  static void checkGradientsAndThermodynamicForcesConsistency(
      ErrorHandler &raise,
      const std::vector<Variable> &gradients,
      const std::vector<Variable> &thermodynamic_forces,
      const Variable &g,
      const Variable &t) {
    auto raise_if = [&raise](const bool c, const std::string &m) {
      if (c) {
        raise(m);
      }
    };
    raise_if(gradients.size() != thermodynamic_forces.size(),
             "the number of gradients does not match the number of "
             "thermodynamic forces");
    raise_if(gradients.size() != 1u, "invalid number of gradients");
    raise_if(gradients[0].name != g.name, "invalid gradient name");
    raise_if(gradients[0].type != g.type, "invalid gradient type");
    raise_if(thermodynamic_forces[0].name != t.name,
             "invalid thermodynamic force name");
    raise_if(thermodynamic_forces[0].type != t.type,
             "invalid thermodynamic force type");
  }  // end of checkGradientsAndThermodynamicForcesConsistency

  static std::pair<Variable, Variable> getJacobianBlockVariables(
      const Behaviour &b, const std::pair<std::string, std::string> &block) {
    auto found = false;
    std::pair<Variable, Variable> v;
    auto assign_if = [&found, &block, &v](const Variable &v1,
                                          const Variable &v2) {
      mgis::raise_if(found,
                     "getJacobianBlockVariables: "
                     "multiple definition for block {" +
                         block.first + "," + block.second + "}");
      found = true;
      v = {v1, v2};
    };
    for (const auto &f : b.thermodynamic_forces) {
      for (const auto &g : b.gradients) {
        if ((block.first == f.name) && (block.second == g.name)) {
          assign_if(f, g);
        }
      }
      for (const auto &e : b.esvs) {
        if ((block.first == f.name) && (block.second == e.name)) {
          assign_if(f, e);
        }
      }
    }
    for (const auto &i : b.isvs) {
      for (const auto &g : b.gradients) {
        if ((block.first == i.name) && (block.second == g.name)) {
          assign_if(i, g);
        }
      }
      for (const auto &e : b.esvs) {
        if ((block.first == i.name) && (block.second == e.name)) {
          assign_if(i, e);
        }
      }
    }
    if (!found) {
      mgis::raise(
          "getJacobianBlockVariables: "
          "tangent operator block {" +
          block.first + "," + block.second + "} is invalid");
    }
    return v;
  }  // end of getJacobianBlockVariables

  BehaviourInitializeFunction::BehaviourInitializeFunction() = default;
  BehaviourInitializeFunction::BehaviourInitializeFunction(BehaviourInitializeFunction &&) = default;
  BehaviourInitializeFunction::BehaviourInitializeFunction(const BehaviourInitializeFunction &) = default;
  BehaviourInitializeFunction &BehaviourInitializeFunction::operator=(BehaviourInitializeFunction &&) = default;
  BehaviourInitializeFunction &BehaviourInitializeFunction::operator=(const BehaviourInitializeFunction &) = default;
  BehaviourInitializeFunction::~BehaviourInitializeFunction() = default;

  BehaviourPostProcessing::BehaviourPostProcessing() = default;
  BehaviourPostProcessing::BehaviourPostProcessing(BehaviourPostProcessing &&) = default;
  BehaviourPostProcessing::BehaviourPostProcessing(const BehaviourPostProcessing &) = default;
  BehaviourPostProcessing &BehaviourPostProcessing::operator=(BehaviourPostProcessing &&) = default;
  BehaviourPostProcessing &BehaviourPostProcessing::operator=(const BehaviourPostProcessing &) = default;
  BehaviourPostProcessing::~BehaviourPostProcessing() = default;

  Behaviour::Behaviour() = default;
  Behaviour::Behaviour(Behaviour &&) = default;
  Behaviour::Behaviour(const Behaviour &) = default;
  Behaviour &Behaviour::operator=(Behaviour &&) = default;
  Behaviour &Behaviour::operator=(const Behaviour &) = default;
  Behaviour::~Behaviour() = default;

  bool isStandardFiniteStrainBehaviour(const std::string &l,
                                       const std::string &b) {
    auto &lm = mgis::LibrariesManager::get();
    return (lm.getBehaviourType(l, b) == 2) &&
           (lm.getBehaviourKinematic(l, b) == 3);
  }  // end of isStandardFiniteStrainBehaviour

  static Behaviour load_behaviour(const std::string &l,
                                  const std::string &b,
                                  const Hypothesis h) {
    auto &lm = mgis::LibrariesManager::get();
    const auto fct = b + '_' + toString(h);
    auto raise = [&b, &l](const std::string &msg) {
      mgis::raise("load: " + msg + ".\nError while trying to load behaviour '" +
                  b + "' in library '" + l + "'\n");
    };
    auto raise_if = [&b, &l](const bool c, const std::string &msg) {
      if (c) {
        mgis::raise("load: " + msg +
                    ".\nError while trying to load behaviour '" + b +
                    "' in library '" + l + "'\n");
      }
    };
    auto d = Behaviour{};

    d.library = l;
    d.behaviour = b;
    d.function = fct;
    d.hypothesis = h;
    d.b = lm.getBehaviour(l, b, h);

    if (lm.getMaterialKnowledgeType(l, b) != 1u) {
      raise("entry point '" + b + "' in library " + l + " is not a behaviour");
    }

    if (lm.getAPIVersion(l, b) != MGIS_BEHAVIOUR_API_VERSION) {
      std::string msg("unmatched API version\n");
      msg += "- the behaviour uses API version ";
      msg += std::to_string(lm.getAPIVersion(l, b)) + "\n";
      msg += "- mgis uses API version ";
      msg += std::to_string(MGIS_BEHAVIOUR_API_VERSION);
      raise(msg);
    }
    d.tfel_version = lm.getTFELVersion(l, b);
    d.unit_system = lm.getUnitSystem(l, b);
    d.source = lm.getSource(l, b);
    d.btype = [&l, &b, &lm, &raise] {
      /* - 0 : general behaviour
       * - 1 : strain based behaviour *
       * - 2 : standard finite strain behaviour *
       * - 3 : cohesive zone model */
      switch (lm.getBehaviourType(l, b)) {
        case 0:
          return Behaviour::GENERALBEHAVIOUR;
        case 1:
          return Behaviour::STANDARDSTRAINBASEDBEHAVIOUR;
        case 2:
          return Behaviour::STANDARDFINITESTRAINBEHAVIOUR;
        case 3:
          return Behaviour::COHESIVEZONEMODEL;
      }
      raise("unsupported behaviour type");
    }();
    if (d.btype == Behaviour::STANDARDFINITESTRAINBEHAVIOUR) {
      d.options.resize(2, mgis::real(0));
    }
    // behaviour kinematic
    d.kinematic = [&l, &b, &lm, h, &raise] {
      /* - 0: undefined kinematic
       * - 1: standard small strain behaviour kinematic
       * - 2: cohesive zone model kinematic
       * - 3: standard finite strain kinematic (F-Cauchy)
       * - 4: ptest finite strain kinematic (eto-pk1)
       * - 5: Green-Lagrange strain
       * - 6: Miehe Apel Lambrecht logarithmic strain framework */
      switch (lm.getBehaviourKinematic(l, b)) {
        case 0:
          return Behaviour::UNDEFINEDKINEMATIC;
        case 1:
          return Behaviour::SMALLSTRAINKINEMATIC;
        case 2:
          return Behaviour::COHESIVEZONEKINEMATIC;
        case 3:
          return Behaviour::FINITESTRAINKINEMATIC_F_CAUCHY;
        case 4:
          if (((h != Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRAIN) &&
               (h != Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRESS))) {
            raise(
                "invalid hypothesis for behaviour based on "
                "the eto-pk1 kinematic");
          }
          return Behaviour::FINITESTRAINKINEMATIC_ETO_PK1;
      }
      raise("unsupported behaviour kinematic");
    }();
    // setting gradients and thermodynamic forces
    d.gradients = buildVariablesList(raise, lm.getGradientsNames(l, b, h),
                                     lm.getGradientsTypes(l, b, h));
    d.thermodynamic_forces =
        buildVariablesList(raise, lm.getThermodynamicForcesNames(l, b, h),
                           lm.getThermodynamicForcesTypes(l, b, h));
    raise_if(d.gradients.size() != d.thermodynamic_forces.size(),
             "the number of the gradients does not match "
             "the number of thermodynamic forces");
    switch (d.btype) {
      case Behaviour::GENERALBEHAVIOUR:
        break;
      case Behaviour::STANDARDSTRAINBASEDBEHAVIOUR:
        raise_if(d.kinematic != Behaviour::SMALLSTRAINKINEMATIC,
                 "strain based behaviour must be associated with the "
                 "small strain kinematic hypothesis");
        checkGradientsAndThermodynamicForcesConsistency(
            raise, d.gradients, d.thermodynamic_forces,
            {"Strain", Variable::STENSOR, 1}, {"Stress", Variable::STENSOR, 1});
        break;
      case Behaviour::COHESIVEZONEMODEL:
        if (d.kinematic != Behaviour::COHESIVEZONEKINEMATIC) {
          raise("invalid kinematic assumption for cohesive zone model");
        }
        checkGradientsAndThermodynamicForcesConsistency(
            raise, d.gradients, d.thermodynamic_forces,
            {"OpeningDisplacement", Variable::VECTOR, 2},
            {"CohesiveForce", Variable::VECTOR, 2});
        break;
      case Behaviour::STANDARDFINITESTRAINBEHAVIOUR:
        if (d.kinematic == Behaviour::FINITESTRAINKINEMATIC_F_CAUCHY) {
          checkGradientsAndThermodynamicForcesConsistency(
              raise, d.gradients, d.thermodynamic_forces,
              {"DeformationGradient", Variable::TENSOR, 3},
              {"Stress", Variable::STENSOR, 1});
        } else if (d.kinematic == Behaviour::FINITESTRAINKINEMATIC_ETO_PK1) {
          if (((h != Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRAIN) &&
               (h != Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRESS))) {
            raise(
                "invalid hypothesis for behaviour based on "
                "the eto-pk1 kinematic");
          }
          checkGradientsAndThermodynamicForcesConsistency(
              raise, d.gradients, d.thermodynamic_forces,
              {"Strain", Variable::STENSOR, 1},
              {"Stresss", Variable::STENSOR, 1});
        } else {
          raise(
              "invalid kinematic hypothesis for finite strain "
              "behaviour");
        }
        break;
      default:
        raise("unsupported behaviour type");
    };
    // behaviour symmetry
    d.symmetry = lm.getBehaviourSymmetry(l, b) == 0 ? Behaviour::ISOTROPIC
                                                    : Behaviour::ORTHOTROPIC;
    auto add_mp = [&d](const std::string &mp) {
      d.mps.push_back({mp, Variable::SCALAR});
    };
    if (lm.requiresStiffnessTensor(l, b, h)) {
      if (lm.getElasticStiffnessSymmetry(l, b) == 0) {
        add_mp("YoungModulus");
        add_mp("PoissonRatio");
      } else {
        if (d.symmetry != Behaviour::ORTHOTROPIC) {
          raise(
              "load: the behaviour must be orthotropic "
              "for the elastic stiffness symmetry to be orthotropic");
        }
        add_mp("YoungModulus1");
        add_mp("YoungModulus2");
        add_mp("YoungModulus3");
        add_mp("PoissonRatio12");
        add_mp("PoissonRatio23");
        add_mp("PoissonRatio13");
        if ((h == Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRAIN) ||
            (h == Hypothesis::AXISYMMETRICALGENERALISEDPLANESTRESS)) {
        } else if ((h == Hypothesis::PLANESTRESS) ||
                   (h == Hypothesis::PLANESTRAIN) ||
                   (h == Hypothesis::AXISYMMETRICAL) ||
                   (h == Hypothesis::GENERALISEDPLANESTRAIN)) {
          add_mp("ShearModulus12");
        } else if (h == Hypothesis::TRIDIMENSIONAL) {
          add_mp("ShearModulus12");
          add_mp("ShearModulus23");
          add_mp("ShearModulus13");
        }
      }
    }
    if (lm.requiresThermalExpansionCoefficientTensor(l, b, h)) {
      if (d.symmetry == Behaviour::ORTHOTROPIC) {
        add_mp("ThermalExpansion1");
        add_mp("ThermalExpansion2");
        add_mp("ThermalExpansion3");
      } else {
        add_mp("ThermalExpansion");
      }
    }
    // standard material properties
    for (const auto &mp : lm.getMaterialPropertiesNames(l, b, h)) {
      add_mp(mp);
    }
    // internal state variables
    d.isvs =
        buildVariablesList(raise, lm.getInternalStateVariablesNames(l, b, h),
                           lm.getInternalStateVariablesTypes(l, b, h));
    // external state variables
    if (lm.hasTemperatureBeenRemovedFromExternalStateVariables(l, b)) {
      d.esvs.push_back({"Temperature", Variable::SCALAR});
    }
    if (lm.hasExternalStateVariablesTypes(l, b, h)) {
      const auto esvs =
          buildVariablesList(raise, lm.getExternalStateVariablesNames(l, b, h),
                             lm.getExternalStateVariablesTypes(l, b, h));
      d.esvs.insert(d.esvs.end(), esvs.begin(), esvs.end());
    } else {
      // Prior to TFEL versions 3.4.4 and 4.1, external state variables were
      // only scalars
      for (const auto &esv : lm.getExternalStateVariablesNames(l, b, h)) {
        d.esvs.push_back({esv, Variable::SCALAR});
      }
    }
    // tangent operator blocks
    for (const auto &block : lm.getTangentOperatorBlocksNames(l, b, h)) {
      d.to_blocks.push_back(getJacobianBlockVariables(d, block));
    }
    d.computesStoredEnergy = lm.computesStoredEnergy(l, b, h);
    d.computesDissipatedEnergy = lm.computesDissipatedEnergy(l, b, h);
    //! parameters
    const auto pn = lm.getParametersNames(l, b, h);
    const auto pt = lm.getParametersTypes(l, b, h);
    raise_if(pn.size() != pt.size(),
             "inconsistent size between parameters' names and"
             "parameters' sizes");
    for (decltype(pn.size()) i = 0; i != pn.size(); ++i) {
      if (pt[i] == 0) {
        d.params.push_back(pn[i]);
      } else if (pt[i] == 1) {
        d.iparams.push_back(pn[i]);
      } else if (pt[i] == 2) {
        d.usparams.push_back(pn[i]);
      } else {
        raise("unsupported parameter type for parameter '" + pn[i] + "'");
      }
    }
    // initialize functions
    for (const auto i : lm.getBehaviourInitializeFunctions(l, b, h)) {
      BehaviourInitializeFunction ifct;
      ifct.f = lm.getBehaviourInitializeFunction(l, b, i, h);
      ifct.inputs = buildVariablesList(
          raise, lm.getBehaviourInitializeFunctionInputsNames(l, b, i, h),
          lm.getBehaviourInitializeFunctionInputsTypes(l, b, i, h));
      d.initialize_functions.insert({i, ifct});
    }
    // post-processings
    for (const auto i : lm.getBehaviourPostProcessings(l, b, h)) {
      BehaviourPostProcessing pfct;
      pfct.f = lm.getBehaviourPostProcessing(l, b, i, h);
      pfct.outputs = buildVariablesList(
          raise, lm.getBehaviourPostProcessingOutputsNames(l, b, i, h),
          lm.getBehaviourPostProcessingOutputsTypes(l, b, i, h));
      d.postprocessings.insert({i, pfct});
    }
    return d;
  }  // end of load_behaviour

  Behaviour load(const std::string &l,
                 const std::string &b,
                 const Hypothesis h) {
    if (isStandardFiniteStrainBehaviour(l, b)) {
      mgis::raise(
          "mgis::behaviour::load: "
          "This version of the load function shall not be called "
          "for finite strain behaviour: you shall specify finite "
          "strain options");
    }
    auto d = load_behaviour(l, b, h);
    if (d.symmetry == Behaviour::ORTHOTROPIC) {
      auto &lm = mgis::LibrariesManager::get();
      d.rotate_gradients_ptr = lm.getRotateBehaviourGradientsFunction(l, b, h);
      d.rotate_array_of_gradients_ptr =
          lm.getRotateArrayOfBehaviourGradientsFunction(l, b, h);
      d.rotate_thermodynamic_forces_ptr =
	lm.getRotateBehaviourThermodynamicForcesFunction(l, b, h);
      d.rotate_array_of_thermodynamic_forces_ptr =
	lm.getRotateArrayOfBehaviourThermodynamicForcesFunction(l, b, h);
      d.rotate_tangent_operator_blocks_ptr =
	lm.getRotateBehaviourTangentOperatorBlocksFunction(l, b, h);
      d.rotate_array_of_tangent_operator_blocks_ptr =
	lm.getRotateArrayOfBehaviourTangentOperatorBlocksFunction(l, b, h);
    }
    return d;
  }  // end of load

  Behaviour load(const FiniteStrainBehaviourOptions &o,
                 const std::string &l,
                 const std::string &b,
                 const Hypothesis h) {
    auto d = load_behaviour(l, b, h);
    if (d.btype != Behaviour::STANDARDFINITESTRAINBEHAVIOUR) {
      mgis::raise(
          "mgis::behaviour::load: "
          "This method shall only be called for finite strain behaviour");
    }
    if (o.stress_measure == FiniteStrainBehaviourOptions::CAUCHY) {
      d.options[0] = mgis::real(0);
    } else if (o.stress_measure == FiniteStrainBehaviourOptions::PK2) {
      d.options[0] = mgis::real(1);
      d.thermodynamic_forces[0] = {"SecondPiolaKirchhoffStress",
                                   Variable::STENSOR, 1};
    } else if (o.stress_measure == FiniteStrainBehaviourOptions::PK1) {
      d.options[0] = mgis::real(2);
      d.thermodynamic_forces[0] = {"FirstPiolaKirchhoffStress",
                                   Variable::TENSOR, 3};
    } else {
      mgis::raise(
          "mgis::behaviour::load: "
          "internal error (unsupported stress measure)");
    }
    if (o.tangent_operator == FiniteStrainBehaviourOptions::DSIG_DF) {
      d.options[1] = mgis::real(0);
    } else if (o.tangent_operator == FiniteStrainBehaviourOptions::DS_DEGL) {
      d.options[1] = mgis::real(1);
      d.to_blocks[0] = {{"SecondPiolaKirchhoffStress", Variable::STENSOR, 1},
                        {"GreenLagrangeStrain", Variable::STENSOR, 1}};

    } else if (o.tangent_operator == FiniteStrainBehaviourOptions::DPK1_DF) {
      d.options[1] = mgis::real(2);
      d.to_blocks[0] = {{"FirstPiolaKirchhoffStress", Variable::TENSOR, 3},
                        {"DeformationGradient", Variable::TENSOR, 3}};
    } else if (o.tangent_operator == FiniteStrainBehaviourOptions::DTAU_DDF) {
      d.options[1] = mgis::real(3);
      d.to_blocks[0] = {{"KirchhoffStress", Variable::STENSOR, 3},
                        {"SpatialIncrementOfTheDeformationGradient", Variable::TENSOR, 3}};
    } else {
      mgis::raise(
          "mgis::behaviour::load: "
          "internal error (unsupported tangent operator)");
    }
    if (d.symmetry == Behaviour::ORTHOTROPIC) {
      auto &lm = mgis::LibrariesManager::get();
      d.rotate_gradients_ptr = lm.getRotateBehaviourGradientsFunction(l, b, h);
      d.rotate_array_of_gradients_ptr =
          lm.getRotateArrayOfBehaviourGradientsFunction(l, b, h);
      d.rotate_thermodynamic_forces_ptr =
          lm.getRotateBehaviourThermodynamicForcesFunction(l, b, h,
                                                           o.stress_measure);
      d.rotate_array_of_thermodynamic_forces_ptr =
          lm.getRotateArrayOfBehaviourThermodynamicForcesFunction(
              l, b, h, o.stress_measure);
      d.rotate_tangent_operator_blocks_ptr =
          lm.getRotateBehaviourTangentOperatorBlocksFunction(
              l, b, h, o.tangent_operator);
      d.rotate_array_of_tangent_operator_blocks_ptr =
          lm.getRotateArrayOfBehaviourTangentOperatorBlocksFunction(
              l, b, h, o.tangent_operator);
    }
    return d;
  }  // end of load

  mgis::size_type getTangentOperatorArraySize(const Behaviour &b) {
    auto s = mgis::size_type{};
    for (const auto &block : b.to_blocks) {
      s += getVariableSize(block.first, b.hypothesis) *
           getVariableSize(block.second, b.hypothesis);
    }
    return s;
  }  // end of getTangentOperatorArraySize

  void rotateGradients(mgis::span<real> g,
                       const Behaviour &b,
                       const mgis::span<const real> &r) {
    rotateGradients(g, b, g, r);
  }  // end of rotateGradients

  void rotateGradients(mgis::span<real> g,
                       const Behaviour &b,
                       const RotationMatrix2D &r) {
    rotateGradients(g, b, g, r);
  }  // end of rotateGradients

  void rotateGradients(mgis::span<real> g,
                       const Behaviour &b,
                       const RotationMatrix3D &r) {
    rotateGradients(g, b, g, r);
  }  // end of rotateGradients

  static mgis::size_type checkRotateFunctionInputs(
      const char *const m,
      const mgis::span<const real> &mv,
      const mgis::span<const real> &gv,
      const mgis::size_type vsize) {
    if (gv.size() == 0) {
      mgis::raise(std::string(m) + ": no values given for the gradients");
    }
    auto dv = std::div(gv.size(), vsize);
    if (dv.rem != 0) {
      mgis::raise(std::string(m) +
                  ": invalid array size in the global frame "
                  "(not a multiple of the gradients size)");
    }
    if (mv.size() != gv.size()) {
      mgis::raise(std::string(m) + ": unmatched array sizes");
    }
    return dv.quot;
  }

  static void checkRotationMatrix2D(const char *const m,
                                    const RotationMatrix2D &r,
                                    const Behaviour &b,
                                    const mgis::size_type nipts) {
    if (getSpaceDimension(b.hypothesis) != 2u) {
      mgis::raise(std::string(m) + ": a 2D rotation matrix can't be used in '" +
                  toString(b.hypothesis) + "'");
    }
    if (r.a.size() != 2u) {
      if (nipts != r.a.size() % 2) {
        mgis::raise(std::string(m) +
                    ": the number of integration points handled "
                    "by the rotation matrix is different from the "
                    "number of integration points of the field to be rotated");
      }
    }
  }  // end of checkRotationMatrix2D

  static void checkRotationMatrix3D(const char *const m,
                                    const RotationMatrix3D &r,
                                    const Behaviour &b,
                                    const mgis::size_type nipts) {
    if (getSpaceDimension(b.hypothesis) != 3u) {
      mgis::raise(std::string(m) + ": a 3D rotation matrix can't be used in '" +
                  toString(b.hypothesis) + "'");
    }
    if (r.a1.a.size() != 3u) {
      if (nipts != r.a1.a.size() % 3) {
        mgis::raise(std::string(m) +
                    ": the number of integration points handled "
                    "by the rotation matrix is different from the "
                    "number of integration points of the field to be rotated");
      }
    }
    if (r.a2.a.size() != 3u) {
      if (nipts != r.a2.a.size() % 3) {
        mgis::raise(std::string(m) +
                    ": the number of integration points handled "
                    "by the rotation matrix is different from the "
                    "number of integration points of the field to be rotated");
      }
    }
  }  // end of checkRotationMatrix3D

  static void checkBehaviourRotateGradients(const Behaviour &b) {
    if ((b.rotate_gradients_ptr == nullptr) ||
        (b.rotate_array_of_gradients_ptr == nullptr)) {
      mgis::raise(
          "rotateGradients: no function performing the rotation of "
          "the gradients defined");
    }
  }  // end of checkBehaviourRotateGradients

  void rotateGradients(mgis::span<real> mg,
                       const Behaviour &b,
                       const mgis::span<const real> &gg,
                       const mgis::span<const real> &r) {
    checkBehaviourRotateGradients(b);
    const auto gsize = getArraySize(b.gradients, b.hypothesis);
    const auto nipts =
        checkRotateFunctionInputs("rotateGradients", mg, gg, gsize);
    if (r.size() == 0) {
      mgis::raise("rotateGradients: no values given for the rotation matrices");
    }
    const auto rdv = std::div(r.size(), size_type{9});
    if (rdv.rem != 0) {
      mgis::raise(
          "rotateGradients: "
          "invalid size for the rotation matrix array");
    }
    if (rdv.quot == 1) {
      b.rotate_array_of_gradients_ptr(mg.data(), gg.data(), r.data(), nipts);
    } else {
      if (rdv.quot != nipts) {
        mgis::raise(
            "the number of integration points for the gradients does not match "
            "the number of integration points for the rotation matrices (" +
            std::to_string(nipts) + " vs " + std::to_string(rdv.quot) + ")");
      }
      for (size_type i = 0; i != nipts; ++i) {
        const auto o = i * gsize;
        b.rotate_gradients_ptr(mg.data() + o, gg.data() + o, r.data() + i * 9);
      }
    }
  }  // end of rotateGradients

  void rotateGradients(mgis::span<real> mg,
                       const Behaviour &b,
                       const mgis::span<const real> &gg,
                       const RotationMatrix2D &r) {
    checkBehaviourRotateGradients(b);
    const auto gsize = getArraySize(b.gradients, b.hypothesis);
    const auto nipts =
        checkRotateFunctionInputs("rotateGradients", mg, gg, gsize);
    checkRotationMatrix2D("rotateGradients", r, b, nipts);
    if (r.a.size() == 2u) {
      const auto m = buildRotationMatrix(r.a.data());
      b.rotate_array_of_gradients_ptr(mg.data(), gg.data(), m.data(), nipts);
    } else {
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a.data() + 2 * i);
        const auto o = i * gsize;
        b.rotate_gradients_ptr(mg.data() + o, gg.data() + o, m.data());
      }
    }
  }  // end of rotateGradients

  void rotateGradients(mgis::span<real> mg,
                       const Behaviour &b,
                       const mgis::span<const real> &gg,
                       const RotationMatrix3D &r) {
    checkBehaviourRotateGradients(b);
    const auto gsize = getArraySize(b.gradients, b.hypothesis);
    const auto nipts =
        checkRotateFunctionInputs("rotateGradients", mg, gg, gsize);
    checkRotationMatrix3D("rotateGradients", r, b, nipts);
    if ((r.a1.a.size() == 3u) && ((r.a2.a.size() == 3u))) {
      const auto m = buildRotationMatrix(r.a1.a.data(), r.a2.a.data());
      b.rotate_array_of_gradients_ptr(mg.data(), gg.data(), m.data(), nipts);
    } else {
      const auto o1 = (r.a1.a.size() == 3u) ? 0u : 3u;
      const auto o2 = (r.a2.a.size() == 3u) ? 0u : 3u;
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a1.a.data() + o1 * i,  //
                                           r.a2.a.data() + o2 * i);
        const auto o = i * gsize;
        b.rotate_gradients_ptr(mg.data() + o, gg.data() + o, m.data());
      }
    }
  }  // end of rotateGradients

  void rotateThermodynamicForces(mgis::span<real> tf,
                                 const Behaviour &b,
                                 const mgis::span<const real> &r) {
    rotateThermodynamicForces(tf, b, tf, r);
  }  // end of rotateThermodynamicForces

  void rotateThermodynamicForces(mgis::span<real> tf,
                                 const Behaviour &b,
                                 const RotationMatrix2D &r) {
    rotateThermodynamicForces(tf, b, tf, r);
  }  // end of rotateThermodynamicForces

  void rotateThermodynamicForces(mgis::span<real> tf,
                                 const Behaviour &b,
                                 const RotationMatrix3D &r) {
    rotateThermodynamicForces(tf, b, tf, r);
  }  // end of rotateThermodynamicForces

  static void checkBehaviourRotateThermodynamicForces(const Behaviour &b) {
    if ((b.rotate_thermodynamic_forces_ptr == nullptr) ||
        (b.rotate_array_of_thermodynamic_forces_ptr == nullptr)) {
      mgis::raise(
          "rotateThermodynamicForces: no function performing the rotation of "
          "the thermodynamic forces defined");
    }
  }  // end of checkBehaviourRotateThermodynamicForces

  void rotateThermodynamicForces(mgis::span<real> gtf,
                                 const Behaviour &b,
                                 const mgis::span<const real> &mtf,
                                 const mgis::span<const real> &r) {
    checkBehaviourRotateThermodynamicForces(b);
    const auto tfsize = getArraySize(b.thermodynamic_forces, b.hypothesis);
    const auto nipts = checkRotateFunctionInputs("rotateThermodynamicForces",
                                                 mtf, gtf, tfsize);
    if (r.size() == 0) {
      mgis::raise(
          "rotateThermodynamicForces: "
          "no values given for the rotation matrices");
    }
    auto rdv = std::div(r.size(), size_type{9});
    if (rdv.rem != 0) {
      mgis::raise(
          "rotateThermodynamicForces: "
          "invalid size for the rotation matrix array");
    }
    if (rdv.quot == 1) {
      b.rotate_array_of_thermodynamic_forces_ptr(gtf.data(), mtf.data(),
                                                 r.data(), nipts);
    } else {
      if (rdv.quot != nipts) {
        mgis::raise(
            "rotateThermodynamicForces: "
            "the number of integration points for the thermodynamic forces "
            "does not match the number of integration points for the rotation "
            "matrices (" +
            std::to_string(nipts) + " vs " + std::to_string(rdv.quot) + ")");
      }
      for (size_type i = 0; i != nipts; ++i) {
        const auto o = i * tfsize;
        b.rotate_thermodynamic_forces_ptr(gtf.data() + o, mtf.data() + o,
                                          r.data() + i * 9);
      }
    }
  }  // end of rotateThermodynamicForces

  void rotateThermodynamicForces(mgis::span<real> gtf,
                                 const Behaviour &b,
                                 const mgis::span<const real> &mtf,
                                 const RotationMatrix2D &r) {
    checkBehaviourRotateThermodynamicForces(b);
    const auto tfsize = getArraySize(b.thermodynamic_forces, b.hypothesis);
    const auto nipts = checkRotateFunctionInputs("rotateThermodynamicForces",
                                                 gtf, mtf, tfsize);
    checkRotationMatrix2D("rotateThermodynamicForces", r, b, nipts);
    if (r.a.size() == 2u) {
      const auto m = buildRotationMatrix(r.a.data());
      b.rotate_array_of_thermodynamic_forces_ptr(gtf.data(), mtf.data(),
                                                 m.data(), nipts);
    } else {
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a.data() + 2 * i);
        const auto o = i * tfsize;
        b.rotate_thermodynamic_forces_ptr(gtf.data() + o, mtf.data() + o,
                                          m.data());
      }
    }
  }  // end of rotateThermodynamicForces

  void rotateThermodynamicForces(mgis::span<real> gtf,
                                 const Behaviour &b,
                                 const mgis::span<const real> &mtf,
                                 const RotationMatrix3D &r) {
    checkBehaviourRotateThermodynamicForces(b);
    const auto tfsize = getArraySize(b.thermodynamic_forces, b.hypothesis);
    const auto nipts = checkRotateFunctionInputs("rotateThermodynamicForces",
                                                 gtf, mtf, tfsize);
    checkRotationMatrix3D("rotateThermodynamicForces", r, b, nipts);
    if ((r.a1.a.size() == 3u) && ((r.a2.a.size() == 3u))) {
      const auto m = buildRotationMatrix(r.a1.a.data(), r.a2.a.data());
      b.rotate_array_of_thermodynamic_forces_ptr(gtf.data(), mtf.data(),
                                                 m.data(), nipts);
    } else {
      const auto o1 = (r.a1.a.size() == 3u) ? 0u : 3u;
      const auto o2 = (r.a2.a.size() == 3u) ? 0u : 3u;
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a1.a.data() + o1 * i,  //
                                           r.a2.a.data() + o2 * i);
        const auto o = i * tfsize;
        b.rotate_thermodynamic_forces_ptr(gtf.data() + o, mtf.data() + o,
                                          m.data());
      }
    }
  }  // end of rotateThermodynamicForces

  void rotateTangentOperatorBlocks(mgis::span<real> K,
                                   const Behaviour &b,
                                   const mgis::span<const real> &r) {
    rotateTangentOperatorBlocks(K, b, K, r);
  }  // end of rotateTangentOperatorBlocks

  void rotateTangentOperatorBlocks(mgis::span<real> K,
                                   const Behaviour &b,
                                   const RotationMatrix2D &r) {
    rotateTangentOperatorBlocks(K, b, K, r);
  }  // end of rotateTangentOperatorBlocks

  void rotateTangentOperatorBlocks(mgis::span<real> K,
                                   const Behaviour &b,
                                   const RotationMatrix3D &r) {
    rotateTangentOperatorBlocks(K, b, K, r);
  }  // end of rotateTangentOperatorBlocks

  static void checkBehaviourRotateTangentOperatorBlocks(const Behaviour &b) {
    if ((b.rotate_tangent_operator_blocks_ptr == nullptr) ||
        (b.rotate_array_of_tangent_operator_blocks_ptr == nullptr)) {
      mgis::raise(
          "rotateTangentOperatorBlocks: no function performing the rotation of "
          "the thermodynamic forces defined");
    }
  }  // end of checkBehaviourRotateTangentOperatorBlocks

  void rotateTangentOperatorBlocks(mgis::span<real> gK,
                                   const Behaviour &b,
                                   const mgis::span<const real> &mK,
                                   const mgis::span<const real> &r) {
    checkBehaviourRotateTangentOperatorBlocks(b);
    const auto Ksize = getTangentOperatorArraySize(b);
    const auto nipts =
        checkRotateFunctionInputs("rotateTangentOperatorBlocks", mK, gK, Ksize);
    if (r.size() == 0) {
      mgis::raise(
          "rotateTangentOperatorBlocks: "
          "empty array for the rotation matrix");
    }
    if (mK.size() != gK.size()) {
      mgis::raise("rotateTangentOperatorBlocks: unmatched array sizes");
    }
    auto rdv = std::div(r.size(), size_type{9});
    if (rdv.rem != 0) {
      mgis::raise(
          "rotateTangentOperatorBlocks: "
          "invalid size for the rotation matrix array");
    }
    if (rdv.quot == 1) {
      b.rotate_array_of_tangent_operator_blocks_ptr(gK.data(), mK.data(),
                                                    r.data(), nipts);
    } else {
      if (rdv.quot != nipts) {
        mgis::raise(
            "the number of integration points for the tangent operators does "
            "not match the number of integration points for the rotation "
            "matrices (" +
            std::to_string(nipts) + " vs " + std::to_string(rdv.quot) + ")");
      }
      for (size_type i = 0; i != nipts; ++i) {
        const auto o = i * Ksize;
        b.rotate_tangent_operator_blocks_ptr(gK.data() + o, mK.data() + o,
                                             r.data() + 9 * i);
      }
    }
  }  // end of rotateTangentOperatorBlocks

  void rotateTangentOperatorBlocks(mgis::span<real> gK,
                                   const Behaviour &b,
                                   const mgis::span<const real> &mK,
                                   const RotationMatrix2D &r) {
    checkBehaviourRotateTangentOperatorBlocks(b);
    const auto Ksize = getTangentOperatorArraySize(b);
    const auto nipts =
        checkRotateFunctionInputs("rotateTangentOperatorBlocks", gK, mK, Ksize);
    checkRotationMatrix2D("rotateTangentOperatorBlocks", r, b, nipts);
    if (r.a.size() == 2u) {
      const auto m = buildRotationMatrix(r.a.data());
      b.rotate_array_of_tangent_operator_blocks_ptr(gK.data(), mK.data(),
                                                    m.data(), nipts);
    } else {
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a.data() + 2 * i);
        const auto o = i * Ksize;
        b.rotate_tangent_operator_blocks_ptr(gK.data() + o, mK.data() + o,
                                             m.data());
      }
    }
  }  // end of rotateTangentOperatorBlocks

  void rotateTangentOperatorBlocks(mgis::span<real> gK,
                                   const Behaviour &b,
                                   const mgis::span<const real> &mK,
                                   const RotationMatrix3D &r) {
    checkBehaviourRotateTangentOperatorBlocks(b);
    const auto Ksize = getTangentOperatorArraySize(b);
    const auto nipts =
        checkRotateFunctionInputs("rotateTangentOperatorBlocks", gK, mK, Ksize);
    checkRotationMatrix3D("rotateTangentOperatorBlocks", r, b, nipts);
    if ((r.a1.a.size() == 3u) && ((r.a2.a.size() == 3u))) {
      const auto m = buildRotationMatrix(r.a1.a.data(), r.a2.a.data());
      b.rotate_array_of_tangent_operator_blocks_ptr(gK.data(), mK.data(),
                                                    m.data(), nipts);
    } else {
      const auto o1 = (r.a1.a.size() == 3u) ? 0u : 3u;
      const auto o2 = (r.a2.a.size() == 3u) ? 0u : 3u;
      for (size_type i = 0; i != nipts; ++i) {
        const auto m = buildRotationMatrix(r.a1.a.data() + o1 * i,  //
                                           r.a2.a.data() + o2 * i);
        const auto o = i * Ksize;
        b.rotate_tangent_operator_blocks_ptr(gK.data() + o, mK.data() + o,
                                             m.data());
      }
    }
  }  // end of rotateTangentOperatorBlocks

  void setParameter(const Behaviour &b, const std::string &n, const double v) {
    auto &lm = mgis::LibrariesManager::get();
    lm.setParameter(b.library, b.behaviour, b.hypothesis, n, v);
  }  // end of setParameter

  void setParameter(const Behaviour &b, const std::string &n, const int v) {
    auto &lm = mgis::LibrariesManager::get();
    lm.setParameter(b.library, b.behaviour, b.hypothesis, n, v);
  }  // end of setParameter

  void setParameter(const Behaviour &b,
                    const std::string &n,
                    const unsigned short v) {
    auto &lm = mgis::LibrariesManager::get();
    lm.setParameter(b.library, b.behaviour, b.hypothesis, n, v);
  }  // end of setParameter

  template <>
  double getParameterDefaultValue<double>(const Behaviour &b,
                                          const std::string &n) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getParameterDefaultValue(b.library, b.behaviour, b.hypothesis, n);
  }  // end of getParameterDefaultValue<double>

  template <>
  int getParameterDefaultValue<int>(const Behaviour &b, const std::string &n) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getIntegerParameterDefaultValue(b.library, b.behaviour,
                                              b.hypothesis, n);
  }  // end of getParameterDefaultValue<int>

  template <>
  unsigned short getParameterDefaultValue<unsigned short>(
      const Behaviour &b, const std::string &n) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getUnsignedShortParameterDefaultValue(b.library, b.behaviour,
                                                    b.hypothesis, n);
  }  // end of getParameterDefaultValue<unsigned short>

  bool hasBounds(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasBounds(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasBounds

  bool hasLowerBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasLowerBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasLowerBound

  bool hasUpperBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasUpperBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasUpperBound

  long double getLowerBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getLowerBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of getLowerBound

  long double getUpperBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getUpperBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of getUpperBound

  bool hasPhysicalBounds(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasPhysicalBounds(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasPhysicalBounds

  bool hasLowerPhysicalBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasLowerPhysicalBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasLowerPhysicalBound

  bool hasUpperPhysicalBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.hasUpperPhysicalBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of hasUpperPhysicalBound

  long double getLowerPhysicalBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getLowerPhysicalBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of getLowerPhysicalBound

  long double getUpperPhysicalBound(const Behaviour &b, const std::string &v) {
    auto &lm = mgis::LibrariesManager::get();
    return lm.getUpperPhysicalBound(b.library, b.behaviour, b.hypothesis, v);
  }  // end of getUpperPhysicalBound

  void print_markdown(std::ostream &,
                      const Behaviour &,
                      const mgis::size_type) {}  // end of print_markdown

  size_type getInitializeFunctionVariablesArraySize(const Behaviour &b,
                                                   const std::string_view n) {
    const auto p = b.initialize_functions.find(n);
    if (p == b.initialize_functions.end()) {
      mgis::raise(
          "getInitializeFunctionVariables: "
          "no initialize function named '" +
          std::string{n} + "'");
    }
    return getArraySize(p->second.inputs, b.hypothesis);
  }  // end of getInitializeFunctionVariablesArraySize

  std::vector<mgis::real> allocateInitializeFunctionVariables(
      const Behaviour &b, const std::string_view n) {
    const auto s = getInitializeFunctionVariablesArraySize(b, n);
    std::vector<mgis::real> inputs;
    inputs.resize(s, real{0});
    return inputs;
  }  // end of allocateInitializeFunctionVariables

  size_type getPostProcessingVariablesArraySize(const Behaviour &b,
                                                const std::string_view n) {
    const auto p = b.postprocessings.find(n);
    if (p == b.postprocessings.end()) {
      mgis::raise(
          "getPostProcessingVariables: "
          "no post-processing named '" +
          std::string{n} + "'");
    }
    return getArraySize(p->second.outputs, b.hypothesis);
  }  // end of getPostProcessingVariablesArraySize

  std::vector<mgis::real> allocatePostProcessingVariables(
      const Behaviour &b, const std::string_view n) {
    const auto s = getPostProcessingVariablesArraySize(b, n);
    std::vector<mgis::real> outputs;
    outputs.resize(s, real{0});
    return outputs;
  }

}  // end of namespace mgis::behaviour

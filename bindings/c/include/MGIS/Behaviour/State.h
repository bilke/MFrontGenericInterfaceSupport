/*!
 * \file   include/MGIS/Behaviour/State.h
 * \brief
 * \author Thomas Helfer
 * \date   02/08/2018
 * \copyright (C) Copyright Thomas Helfer 2018.
 * Use, modification and distribution are subject
 * to one of the following licences:
 * - GNU Lesser General Public License (LGPL), Version 3.0. (See accompanying
 *   file LGPL-3.0.txt)
 * - CECILL-C,  Version 1.0 (See accompanying files
 *   CeCILL-C_V1-en.txt and CeCILL-C_V1-fr.txt).
 */

#ifndef LIB_MGIS_BEHAVIOUR_STATE_H
#define LIB_MGIS_BEHAVIOUR_STATE_H

#include "MGIS/Status.h"
#include "MGIS/Behaviour/Behaviour.h"

#ifdef __cplusplus
#include "MGIS/Behaviour/State.hxx"
#endif /*  __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /*  __cplusplus */

#ifdef __cplusplus
using mgis_bv_State = mgis::behaviour::State;
#else /*  __cplusplus */
/*!
 * \brief an opaque structure which can only be accessed through the MGIS' API.
 */
typedef struct mgis_bv_State mgis_bv_State;
#endif /*  __cplusplus */

/*!
 * \brief set a material property' value in a state
 * \param[out] s: state
 * \param[in]  b: behaviour
 * \param[in]  n: name
 * \param[in]  v: value
 */
MGIS_C_EXPORT mgis_status
mgis_bv_set_state_material_property_by_name(mgis_bv_State* const,
                                            const mgis_bv_Behaviour* const,
                                            const char* const,
                                            const mgis_real);
/*!
 * \brief get a material property' value in a state
 * \param[out] v: material property' value
 * \param[in] s: state
 * \param[in] b: behaviour
 * \param[in] n: name
 */
MGIS_C_EXPORT mgis_status
mgis_bv_get_state_material_property_by_name(mgis_real* const,
                                            const mgis_bv_State* const,
                                            const mgis_bv_Behaviour* const,
                                            const char* const);
/*!
 * \brief set a material property' value in a state
 * \param[out] s: state
 * \param[in] o: offset
 * \param[in] v: value
 */
MGIS_C_EXPORT mgis_status mgis_bv_set_state_material_property_by_offset(
    mgis_bv_State* const, const mgis_size_type, const mgis_real);
/*!
 * \brief get a material property' value in a state
 * \param[out] v: material property' value
 * \param[in] s: state
 * \param[in] n: name
 */
MGIS_C_EXPORT mgis_status mgis_bv_get_state_material_property_by_offset(
    mgis_real* const, const mgis_bv_State* const, const mgis_size_type);
/*!
 * \brief set a external state variable' value in a state
 * \param[out] s: state
 * \param[in] b: behaviour
 * \param[in] n: name
 * \param[in] v: value
 */
MGIS_C_EXPORT mgis_status mgis_bv_set_state_external_state_variable_by_name(
    mgis_bv_State* const,
    const mgis_bv_Behaviour* const,
    const char* const,
    const mgis_real);
/*!
 * \brief get a external state variable' value in a state
 * \param[out] v: external state variable' value
 * \param[in] s: state
 * \param[in] b: behaviour
 * \param[in] n: name
 */
MGIS_C_EXPORT mgis_status mgis_bv_get_state_external_state_variable_by_name(
    mgis_real* const,
    const mgis_bv_State* const,
    const mgis_bv_Behaviour* const,
    const char* const);
/*!
 * \brief set a external state variable' value in a state
 * \param[out] s: state
 * \param[in] o: offset
 * \param[in] v: value
 */
MGIS_C_EXPORT mgis_status mgis_bv_set_state_external_state_variable_by_offset(
    mgis_bv_State* const, const mgis_size_type, const mgis_real);
/*!
 * \brief get a external state variable' value in a state
 * \param[out] v: external state variable' value
 * \param[in]  s: state
 * \param[in]  n: name
 */
MGIS_C_EXPORT mgis_status mgis_bv_get_state_external_state_variable_by_offset(
    mgis_real* const, const mgis_bv_State* const, const mgis_size_type);

#ifdef __cplusplus
}  // end of extern "C"
#endif /*  __cplusplus */

#endif /* LIB_MGIS_BEHAVIOUR_STATE_H */

/*

Copyright (c) 2012, Simon Lynen, ASL, ETH Zurich, Switzerland
You can contact the author at <slynen at ethz dot ch>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ETHZ-ASL nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETHZ-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/



#ifndef MSF_STATE_HPP_
#define MSF_STATE_HPP_

#include <msf_core/msf_types.tpp>
#include <msf_core/msf_tmp.hpp>
#include <msf_core/msf_statedef.hpp>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <vector>
#include <msf_core/eigen_conversions.h>
#include <sensor_fusion_comm/ExtState.h>
#include <sensor_fusion_comm/DoubleArrayStamped.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>

namespace msf_core{

/** \class StateVisitor
 * \brief visitor pattern to allow the user to set state init values
 */
class StateVisitor{
public:
	//the state is set to zero/identity, this method will be called to
	//give the user the possibility to change the reset values of some states
	virtual void resetState(msf_core::EKFState& state)=0;
	virtual ~StateVisitor(){};
};

/** \class StateVar_T
 * \brief a state variable with a name as specified in the state name enum
 */
template<typename type_T, int name_T, int STATETYPE>
struct StateVar_T{
	typedef type_T value_t;
	typedef const StateVar_T<type_T, name_T>& constRef_T;
	typedef const StateVar_T<type_T, name_T>* constPtr_T;
	typedef StateVar_T<type_T, name_T>& Ref_T;
	typedef StateVar_T<type_T, name_T>* Ptr_T;
	enum{
		statetype_ = STATETYPE, ///<the type of this state. needed for computations of total state length
		name_ = name_T, ///< the name of the state, needed to find it in the state type list
		sizeInCorrection_ = msf_tmp::CorrectionStateLengthForType<const StateVar_T<type_T, name_T>&>::value, ///< the size of this state in the correction vector
		sizeInState_ = msf_tmp::StateLengthForType<const StateVar_T<type_T, name_T>&>::value ///<the size of this state in the state vector
	};
	typedef Eigen::Matrix<double, sizeInCorrection_, sizeInCorrection_> Q_T;
	Q_T Q_; ///< the noise covariance matrix block of this state
	value_t state_; ///< the state variable of this state
	bool hasResetValue; //<indicating that this statevariable has a reset value to be applied to the state on init
	StateVar_T(){
		hasResetValue = false;
		Q_.setZero();
	}
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


/** \class GenericState_T
 * \brief the state vector containing all the state variables for this EKF configuration
 */
template<typename stateSequence_T>
struct GenericState_T{
	friend class msf_core::MSF_Core;
	friend class msf_core::copyNonPropagationStates<GenericState_T>;
	friend class msf_core::MSF_InitMeasurement;
public:
	typedef stateSequence_T stateVector_T;
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	enum{
		nStateVarsAtCompileTime = boost::fusion::result_of::size<stateVector_T>::type::value, ///<n state vars
		nErrorStatesAtCompileTime = msf_tmp::CountStates<stateVector_T, msf_tmp::CorrectionStateLengthForType>::value, ///<n error states
		nStatesAtCompileTime = msf_tmp::CountStates<stateVector_T, msf_tmp::StateLengthForType>::value, ///<n total states
		nCoreStatesAtCompileTime = msf_tmp::CountStates<stateVector_T, msf_tmp::CoreStateLengthForType>::value, ///<n total core states
		nPropagatedCoreStatesAtCompileTime = msf_tmp::CountStates<stateVector_T, msf_tmp::PropagatedCoreStateLengthForType>::value, ///<n total core states with propagation
		nPropagatedCoreErrorStatesAtCompileTime = msf_tmp::CountStates<stateVector_T, msf_tmp::PropagatedCoreErrorStateLengthForType>::value ///<n total error states with propagation
	};

private:

	/**
	 * \brief returns the stateVar at position INDEX in the state list
	 * non const version only for msf_core use. You must not make these functions public.
	 * Instead const_cast the state object to const to use the overload
	 */
	template<int INDEX>
	inline typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type
	getStateVar();

	/**
	 * \brief returns the state at position INDEX in the state list, non const version
	 * you must not make these functions public. Instead const_cast the state object to const to use the overload
	 */
	template<int INDEX>
	inline typename msf_tmp::StripReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t::value_t&
	get();

public:

	typedef Eigen::Matrix<double, nErrorStatesAtCompileTime, nErrorStatesAtCompileTime> P_type; ///< type of the error state covariance matrix

	stateVector_T statevars_; ///< the actual state variables

	// system inputs
	Eigen::Matrix<double,3,1> w_m_;         ///< angular velocity from IMU
	Eigen::Matrix<double,3,1> a_m_;         ///< acceleration from IMU


	double time_; 							///< time of this state estimate
	P_type P_;								///< error state covariance


	GenericState_T(){
		time_ = -1;
		P_.setZero();
	}

	/**
	 * \brief apply the correction vector to all state vars
	 */
	inline void correct(const Eigen::Matrix<double, nErrorStatesAtCompileTime, 1>& correction);

	/**
	 * \brief returns the Q-block of the state at position INDEX in the state list, not allowed for core states
	 */
	template<int INDEX>
	inline typename msf_tmp::StripReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t::Q_T&
	getQBlock();

	/**
	 * \brief returns the Q-block of the state at position INDEX in the state list, also possible for core states, since const
	 */
	template<int INDEX>
	inline const typename msf_tmp::StripReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t::Q_T&
	getQBlock() const;

	/**
	 * \brief reset the state
	 * 3D vectors: 0; quaternion: unit quaternion; scale: 1; time:0; Error covariance: zeros
	 */
	void reset(msf_core::StateVisitor* usercalc);

	/**
	 * \brief write the covariance corresponding to position and attitude to cov
	 */
	void getPoseCovariance(geometry_msgs::PoseWithCovariance::_covariance_type & cov); //boost fusion unfortunately doesn't like this to be const

	/**
	 * \brief assembles a PoseWithCovarianceStamped message from the state
	 * it does not set the header
	 */
	void toPoseMsg(geometry_msgs::PoseWithCovarianceStamped & pose);

	/**
	 * \brief assemble an ExtState message from the state
	 * \note it does not set the header
	 */
	void toExtStateMsg(sensor_fusion_comm::ExtState & state);

	/***
	 * \brief assemble a DoubleArrayStamped message from the state
	 * \note it does not set the header
	 */
	void toFullStateMsg(sensor_fusion_comm::DoubleArrayStamped & state);

	/**
	 * \brief assembles a DoubleArrayStamped message from the state
	 * \note it does not set the header
	 */
	void toCoreStateMsg(sensor_fusion_comm::DoubleArrayStamped & state);


	/**
	 * \brief returns the state at position INDEX in the state list, const version
	 */
	template<int INDEX>
	inline const typename msf_tmp::StripReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t::value_t&
	get() const;

	/**
	 * \brief returns the stateVar at position INDEX in the state list, const version
	 */
	template<int INDEX>
	inline typename msf_tmp::AddConstReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t
	getStateVar() const;

	/**
	 * \brief sets state at position INDEX in the state list, fails for core states at compile time
	 */
	template<int INDEX>
	inline void
	set(const typename msf_tmp::StripConstReference<typename boost::fusion::result_of::at_c<stateVector_T, INDEX >::type>::result_t::value_t& newvalue);
};

/** \class sortStates
 * \brief comparator for the state objects. sorts by time asc
 */
template<typename stateSequence_T>
class sortStates
{
public:
  bool operator() (const GenericState_T<stateSequence_T>& lhs, const GenericState_T<stateSequence_T>&rhs) const
  {
    return (lhs.time_<rhs.time_);
  }
};

}

#include <msf_core/implementation/msf_state_.hpp>

#endif /* MSF_STATE_HPP_ */
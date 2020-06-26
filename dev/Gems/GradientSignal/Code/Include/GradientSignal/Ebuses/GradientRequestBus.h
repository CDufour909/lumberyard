/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>

namespace GradientSignal
{
    struct GradientSampleParams final
    {
        AZ_CLASS_ALLOCATOR(GradientSampleParams, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(GradientSampleParams, "{DC4B9269-CB3C-4071-989D-C885FB9946A5}");

        GradientSampleParams() {}
        GradientSampleParams(const AZ::Vector3& position) : m_position(position) {}
        AZ::Vector3 m_position = AZ::Vector3(0.0f, 0.0f, 0.0f);
    };

    /**
    * Handles gradient sampling requests based on up to 3 data points such as X,Y,Z
    */
    class GradientRequests
        : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        using MutexType = AZStd::recursive_mutex;

        ////////////////////////////////////////////////////////////////////////

        virtual ~GradientRequests() = default;

        /**
        * Given a certain position, generate a value.  Implementations of this need to be thread-safe without using locks, 
        * as it can get called from both the Main thread and the Vegetation thread simultaneously, and has the potential to cause
        * lock inversion deadlocks.
        * @return a value generated using the position
        */
        virtual float GetValue(const GradientSampleParams& sampleParams) const = 0;

        /**
        * Call to check the hierarchy to see if a given entityId exists in the gradient signal chain
        */
        virtual bool IsEntityInHierarchy(const AZ::EntityId& entityId) const { return false; }
    };

    using GradientRequestBus = AZ::EBus<GradientRequests>;

} // namespace GradientSignal


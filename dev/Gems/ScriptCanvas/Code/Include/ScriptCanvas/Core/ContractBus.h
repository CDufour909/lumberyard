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

#include "Core.h"

#include <AzCore/EBus/EBus.h>

namespace ScriptCanvas
{
    class Contract;
    class Slot;

    namespace Data
    {
        class Type;
    }

    class ContractEvents : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual void OnValidContract(const Contract*, const Slot&, const Slot&) = 0;
        virtual void OnInvalidContract(const Contract*, const Slot&, const Slot&) = 0;
    };

    using ContractEventBus = AZ::EBus<ContractEvents>;

}

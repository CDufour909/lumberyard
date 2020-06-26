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

#include <AzToolsFramework/AssetBrowser/Entries/AssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/Entries/ProductAssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/Search/Filter.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>

#include <QString>

using namespace AzToolsFramework::AssetBrowser;

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        //! Used in combination with Asset Browser Picker to configure selection settings and store selection results
        class AssetSelectionModel
        {
        public:
            AZ_CLASS_ALLOCATOR(AssetSelectionModel, AZ::SystemAllocator, 0)

            AssetSelectionModel();
            ~AssetSelectionModel() = default;

            FilterConstType GetSelectionFilter() const;
            void SetSelectionFilter(FilterConstType filter);

            FilterConstType GetDisplayFilter() const;
            void SetDisplayFilter(FilterConstType filter);

            bool GetMultiselect() const;
            void SetMultiselect(bool multiselect);
            
            const AZStd::vector<AZ::Data::AssetId>& GetSelectedAssetIds() const;
            void SetSelectedAssetIds(const AZStd::vector<AZ::Data::AssetId>& selectedAssetIds);
            void SetSelectedAssetId(const AZ::Data::AssetId& selectedAssetId);
            
            AZStd::vector<const AssetBrowserEntry*>& GetResults();
            const AssetBrowserEntry* GetResult();
            
            bool IsValid() const;

            void SetTitle(const QString& title);

            QString GetTitle() const;

            static AssetSelectionModel AssetTypeSelection(const AZ::Data::AssetType& assetType, bool multiselect = false);
            static AssetSelectionModel AssetTypeSelection(const char* assetTypeName, bool multiselect = false);
            static AssetSelectionModel AssetTypesSelection(const AZStd::vector<AZ::Data::AssetType>& assetTypes, bool multiselect = false);
            static AssetSelectionModel AssetGroupSelection(const char* group, bool multiselect = false);
            static AssetSelectionModel EverythingSelection(bool multiselect = false);

        private:
            bool m_multiselect;

            // some entries like folder should always be displayed, but not always selectable, thus 2 separate filters
            FilterConstType m_selectionFilter;
            FilterConstType m_displayFilter;
            
            AZStd::vector<AZ::Data::AssetId> m_selectedAssetIds;
            AZStd::vector<const AssetBrowserEntry*> m_results;

            QString m_title;
        };
    } // namespace AssetBrowser
} // namespace AzToolsFramework
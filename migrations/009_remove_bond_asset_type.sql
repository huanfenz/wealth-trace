-- Remove the standalone bond asset type while preserving its asset records.
-- Reclassify any existing BOND assets as OTHER, then drop their type-specific details.
UPDATE asset
SET asset_type = 'OTHER'
WHERE asset_type = 'BOND';

DROP TABLE bond_detail;

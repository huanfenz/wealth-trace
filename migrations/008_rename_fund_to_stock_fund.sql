-- Rename the original fund asset type and detail table to stock fund.
-- Keep existing asset records and detail rows intact.
ALTER TABLE fund_detail RENAME TO stock_fund_detail;

UPDATE asset
SET asset_type = 'STOCK_FUND'
WHERE asset_type = 'FUND';

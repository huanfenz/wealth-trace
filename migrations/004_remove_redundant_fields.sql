-- 去重：删除明细表里与资产名称 / 初始金额重复的字段。
--   * 本金统一以 asset.opening_balance 为准，不再在各明细表重复保存 principal；
--   * 基金 / 债券名称统一使用 asset.name，不再保存 fund_name / bond_name。
-- SQLite 3.35+ 支持 DROP COLUMN；这些列均非主键、未被索引、未被外键引用，可直接删除。
ALTER TABLE term_deposit_detail DROP COLUMN principal;
ALTER TABLE bond_detail DROP COLUMN principal;
ALTER TABLE bond_detail DROP COLUMN bond_name;
ALTER TABLE fund_detail DROP COLUMN fund_name;
ALTER TABLE bond_fund_detail DROP COLUMN principal;
ALTER TABLE bond_fund_detail DROP COLUMN fund_name;

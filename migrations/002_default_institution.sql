-- 为现金/支付宝/微信账户回填默认机构名，避免列表「机构」列空着。
-- 仅填补空值，不改动用户已填写的机构名。
UPDATE account
SET institution_name = CASE type
    WHEN 'ALIPAY' THEN '支付宝'
    WHEN 'WECHAT' THEN '微信'
    WHEN 'CASH'   THEN '现金'
    ELSE institution_name
  END
WHERE type IN ('ALIPAY', 'WECHAT', 'CASH')
  AND (institution_name IS NULL OR TRIM(institution_name) = '');

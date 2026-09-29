-- Support household-wide transaction date filtering and daily activity queries.
CREATE INDEX idx_transactions_household_time
ON transactions(household_id, transaction_time);

ALTER TABLE commercial_pension_detail
  RENAME COLUMN change_window_start TO reservation_window_start;
ALTER TABLE commercial_pension_detail
  RENAME COLUMN change_window_end TO reservation_window_end;

ALTER TABLE CHARGING_PROFILES ADD COLUMN CHARGING_LIMIT_SOURCE TEXT NOT NULL DEFAULT 'CSO';
ALTER TABLE CHARGING_PROFILES ADD COLUMN TRANSACTION_ID TEXT;
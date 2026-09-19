#pragma once

#include <nlohmann/json.hpp>

#include "model/entities.hpp"
#include "repository/statistics_repository.hpp"
#include "service/account_service.hpp"
#include "service/asset_service.hpp"
#include "service/statistics_service.hpp"
#include "service/transaction_service.hpp"

namespace wt::dto {

nlohmann::json to_json(const Household& household);
nlohmann::json to_json(const HouseholdMember& member);
nlohmann::json to_json(const Account& account);
nlohmann::json to_json(const AccountView& view);
nlohmann::json to_json(const Asset& asset);
nlohmann::json to_json(const TermDepositDetail& detail);
nlohmann::json to_json(const FundDetail& detail);
nlohmann::json to_json(const BondDetail& detail);
nlohmann::json to_json(const InsuranceDetail& detail);
nlohmann::json to_json(const AssetBundle& bundle);
nlohmann::json to_json(const Transaction& transaction);
nlohmann::json to_json(const TransferResult& result);

nlohmann::json to_json(const NamedAmount& amount);
nlohmann::json to_json(const TypeAmount& amount);
nlohmann::json to_json(const CategoryAmount& amount);
nlohmann::json to_json(const HouseholdOverview& overview);
nlohmann::json to_json(const PeriodStatistics& statistics);

}  // namespace wt::dto

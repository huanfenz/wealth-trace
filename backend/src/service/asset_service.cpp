// 资产服务实现：资产与明细的创建、查询、元数据/状态/明细更新，含事务与校验。
#include "service/asset_service.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "utils/maintenance_rules.hpp"
#include "utils/flexible_term.hpp"
#include "utils/strings.hpp"
#include "utils/term_date.hpp"
#include "utils/time_util.hpp"
#include "utils/commercial_pension.hpp"

namespace wt {
namespace {

std::optional<std::string> clean_optional(const std::optional<std::string>& value,
                                          std::string_view field,
                                          std::size_t max_length) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  const auto cleaned = strings::optional_text(*value, field, max_length);
  if (cleaned.empty()) {
    return std::nullopt;
  }
  return cleaned;
}

// 期初余额符号校验：负债以负数表示（信用卡欠款），普通资产不能为负。
// 该约束保证负债的 current_balance 恒为负，统计时取绝对值即为负债额。
void validate_opening_balance(AssetType type, std::int64_t opening_balance) {
  if (type == AssetType::Liability && opening_balance > 0) {
    throw invalid_request("liability opening_balance must be zero or negative");
  }
  if (type != AssetType::Liability && opening_balance < 0) {
    throw invalid_request("asset opening_balance must be zero or positive");
  }
}

// 推断明细块对应的资产类型：提供了哪个明细就以哪个为准，都没有则用声明的
// asset_type（用于后续「明细必须与 asset_type 一致」的校验）。
AssetType detail_type_of(const AssetCreateInput& input) {
  if (input.term_deposit.has_value()) return AssetType::TermDeposit;
  if (input.stock_fund.has_value()) return AssetType::StockFund;
  if (input.bond_fund.has_value()) return AssetType::BondFund;
  if (input.flexible_term.has_value()) return AssetType::FlexibleTerm;
  if (input.commercial_pension.has_value()) return AssetType::CommercialPension;
  if (input.insurance.has_value()) return AssetType::Insurance;
  return input.asset_type;
}

// 统计入参里提供了几个明细块（用于「一次只能提供一个」的约束）。
int count_provided_details(const AssetCreateInput& input) {
  return (input.term_deposit.has_value() ? 1 : 0) + (input.stock_fund.has_value() ? 1 : 0) +
         (input.bond_fund.has_value() ? 1 : 0) +
         (input.insurance.has_value() ? 1 : 0) + (input.flexible_term.has_value() ? 1 : 0) +
         (input.commercial_pension.has_value() ? 1 : 0);
}

// 校验并补全债券基金的赎回日期。规则见「BOND_FUND 债券基金最终实现方案」：
// - 首次可赎回日期缺省时按 购买日期 + 持有周期 计算，允许调用方手工修正；
// - 持有期型（MIN_HOLDING）next_redeem_date 恒为空；
// - 滚动型（ROLLING）next_redeem_date 缺省时取 existing_next（编辑时沿用已推进的值），
//   否则等于 first_redeem_date。
void normalize_bond_fund(BondFundDetail& detail,
                         const std::optional<std::string>& existing_next) {
  if (detail.holding_period_days <= 0) {
    throw invalid_request("bond fund holding_period_days must be positive");
  }
  detail.purchase_date = time_util::require_date(detail.purchase_date, "purchase_date");
  if (detail.first_redeem_date.has_value()) {
    *detail.first_redeem_date =
        time_util::require_date(*detail.first_redeem_date, "first_redeem_date");
  } else {
    detail.first_redeem_date =
        time_util::add_days(detail.purchase_date, detail.holding_period_days);
  }
  if (detail.next_redeem_date.has_value()) {
    *detail.next_redeem_date =
        time_util::require_date(*detail.next_redeem_date, "next_redeem_date");
  }
  if (detail.maturity_date.has_value()) {
    *detail.maturity_date = time_util::require_date(*detail.maturity_date, "maturity_date");
  }
  if (detail.holding_mode == HoldingMode::Rolling) {
    if (!detail.next_redeem_date.has_value()) {
      detail.next_redeem_date =
          existing_next.has_value() ? existing_next : detail.first_redeem_date;
    }
  } else {
    detail.next_redeem_date = std::nullopt;
  }
}

// 校验并补全定期存款：start_date / term_value / term_unit 必填，
// maturity_date 缺省时按存期（自然月/年）计算，允许用户手工修正。
void normalize_term_deposit(TermDepositDetail& detail) {
  if (!detail.start_date.has_value()) {
    throw invalid_request("term deposit start_date is required");
  }
  *detail.start_date = time_util::require_date(*detail.start_date, "start_date");
  if (!detail.term_value.has_value() || *detail.term_value <= 0) {
    throw invalid_request("term deposit term_value must be positive");
  }
  if (!detail.term_unit.has_value()) {
    throw invalid_request("term deposit term_unit is required");
  }
  if (detail.maturity_date.has_value()) {
    *detail.maturity_date = time_util::require_date(*detail.maturity_date, "maturity_date");
  } else {
    detail.maturity_date =
        time_util::add_term(*detail.start_date, *detail.term_value, *detail.term_unit);
  }
}

void normalize_flexible_term(FlexibleTermDetail& detail) {
  detail.purchase_date = time_util::require_date(detail.purchase_date, "purchase_date");
  if (detail.holding_period_days != 180 && detail.holding_period_days != 360) {
    throw invalid_request("flexible term holding_period_days must be 180 or 360");
  }
}

void normalize_commercial_pension(CommercialPensionDetail& detail,
                                  const std::optional<CommercialPensionDetail>& existing) {
  detail.purchase_time = time_util::require_datetime(detail.purchase_time, "purchase_time");
  if (detail.holding_period_value <= 0 || detail.holding_period_value > 10000) {
    throw invalid_request("holding_period_value must be between 1 and 10000");
  }
  if (detail.reservation_window_start.has_value() != detail.reservation_window_end.has_value()) {
    throw invalid_request("reservation window requires both start and end");
  }
  if (detail.reservation_window_start.has_value()) {
    *detail.reservation_window_start = time_util::require_datetime(
        *detail.reservation_window_start, "reservation_window_start");
    *detail.reservation_window_end = time_util::require_datetime(
        *detail.reservation_window_end, "reservation_window_end");
    if (*detail.reservation_window_start > *detail.reservation_window_end) {
      throw invalid_request("reservation window start must not exceed end");
    }
  }
  const auto now = time_util::utc_to_business(time_util::now_iso8601());
  if (!detail.redeem_at_maturity) {
    detail.redeem_at = std::nullopt;
  } else if (!existing.has_value() || !existing->redeem_at_maturity ||
             detail.purchase_time != existing->purchase_time ||
             detail.holding_period_value != existing->holding_period_value ||
             detail.holding_period_unit != existing->holding_period_unit) {
    detail.redeem_at = commercial_pension::selectable_maturity(detail, now);
  } else {
    detail.redeem_at = existing->redeem_at;
  }
}

}  // namespace

void AssetService::validate_detail_combination(const Asset& asset,
                                               const AssetCreateInput& input) const {
  // 一个资产最多一份明细；有明细时其类型必须与 asset_type 完全一致，
  // 否则会出现「股票基金资产却挂了债券明细」这类不一致数据。
  if (count_provided_details(input) > 1) {
    throw invalid_request("only one detail block may be supplied per asset");
  }
  const AssetType detail_type = detail_type_of(input);
  if (count_provided_details(input) == 1 && detail_type != asset.asset_type) {
    throw invalid_request("detail block does not match asset_type");
  }
}

AssetBundle AssetService::create(std::int64_t household_id, const AssetCreateInput& input) {
  std::scoped_lock lock(database_.mutex());
  const std::string today = time_util::business_today();
  const auto account = accounts_.find_by_id(input.account_id);
  if (!account.has_value()) {
    throw not_found("account not found");
  }
  if (account->household_id != household_id) {
    throw invalid_request("account does not belong to the household");
  }
  std::optional<Asset> payment_source;
  if (input.payment_asset_id.has_value()) {
    if (input.opening_balance <= 0) {
      throw invalid_request("payment requires a positive opening_balance");
    }
    payment_source = assets_.find_by_id(*input.payment_asset_id);
    if (!payment_source.has_value()) {
      throw not_found("payment asset not found");
    }
    if (payment_source->household_id != household_id) {
      throw invalid_request("payment asset does not belong to the household");
    }
    if (payment_source->status != AssetStatus::Active) {
      throw conflict("payment asset must be active");
    }
    if (payment_source->asset_type == AssetType::Liability) {
      throw invalid_request("payment asset cannot be a liability");
    }
    if (payment_source->current_balance < input.opening_balance) {
      throw conflict("payment asset balance is insufficient");
    }
    if (payment_source->asset_type == AssetType::FlexibleTerm) {
      const auto detail = assets_.find_flexible_term_detail(payment_source->id);
      if (!detail.has_value() || !flexible_term::can_transfer(*detail, today)) {
        throw conflict("flexible term asset is not open for transfer today");
      }
    }
    if (payment_source->asset_type == AssetType::CommercialPension) {
      const auto detail = assets_.find_commercial_pension_detail(payment_source->id);
      const auto now = time_util::utc_to_business(time_util::now_iso8601());
      if (!detail.has_value() || !detail->redeem_at_maturity ||
          !detail->redeem_at.has_value() || now < *detail->redeem_at) {
        throw conflict("commercial pension is not yet available for redemption");
      }
    }
  }

  Asset asset;
  // 冗余属主：资产的 household_id/owner_member_id 一律从所属账户复制，
  // 调用方不能自行指定，避免资产与账户归属脱节。
  asset.household_id = account->household_id;
  asset.owner_member_id = account->owner_member_id;
  asset.account_id = account->id;
  asset.name = strings::require_text(input.name, "name", 100);
  asset.asset_type = input.asset_type;
  validate_opening_balance(asset.asset_type, input.opening_balance);
  if (payment_source.has_value() && asset.asset_type == AssetType::Liability) {
    throw invalid_request("liability cannot be paid from another asset");
  }
  // 有付款来源时由买入交易的目标 Entry 建立余额；无付款来源时以期初余额初始化。
  asset.opening_balance = payment_source ? 0 : input.opening_balance;
  asset.current_balance = asset.opening_balance;
  asset.status = AssetStatus::Active;
  asset.remark = clean_optional(input.remark, "remark", 500);
  asset.created_at = time_util::now_iso8601();
  asset.updated_at = asset.created_at;

  validate_detail_combination(asset, input);
  if (asset.asset_type == AssetType::FlexibleTerm && !input.flexible_term.has_value()) {
    throw invalid_request("flexible_term detail is required");
  }
  if (asset.asset_type == AssetType::CommercialPension && !input.commercial_pension.has_value()) {
    throw invalid_request("commercial_pension detail is required");
  }

  // 资产本体与明细分属两张表，必须在同一事务内写入：任一失败则整体回滚，
  // 不会留下「有资产无明细」或「有明细无资产」的中间态。
  TransactionGuard transaction(database_);
  asset.id = assets_.create(asset);

  if (input.term_deposit.has_value()) {
    TermDepositDetail detail = *input.term_deposit;
    detail.asset_id = asset.id;
    normalize_term_deposit(detail);
    // 添加时维护：自动续存的存款若已到期，直接推进到当前存期（与每日维护一致）。
    if (input.maintain_on_create && detail.auto_rollover && detail.maturity_date.has_value()) {
      if (const auto advanced = maintenance_rules::advance_term_period(
              *detail.maturity_date, *detail.term_value, *detail.term_unit, today);
          advanced.has_value()) {
        detail.start_date = advanced->start;
        detail.maturity_date = advanced->maturity;
      }
    }
    assets_.upsert_term_deposit_detail(detail);
  }
  if (input.stock_fund.has_value()) {
    StockFundDetail detail = *input.stock_fund;
    detail.asset_id = asset.id;
    assets_.upsert_stock_fund_detail(detail);
  }
  if (input.bond_fund.has_value()) {
    BondFundDetail detail = *input.bond_fund;
    detail.asset_id = asset.id;
    normalize_bond_fund(detail, std::nullopt);
    // 添加时维护：滚动持有债基的下一赎回日若已过期，直接推进到未来周期。
    if (input.maintain_on_create && detail.next_redeem_date.has_value()) {
      if (const auto advanced = maintenance_rules::advance_bond_fund_next(
              *detail.next_redeem_date, detail.holding_period_days, today);
          advanced.has_value()) {
        detail.next_redeem_date = *advanced;
      }
    }
    assets_.upsert_bond_fund_detail(detail);
  }
  if (input.insurance.has_value()) {
    InsuranceDetail detail = *input.insurance;
    detail.asset_id = asset.id;
    assets_.upsert_insurance_detail(detail);
  }
  if (input.flexible_term.has_value()) {
    FlexibleTermDetail detail = *input.flexible_term;
    detail.asset_id = asset.id;
    normalize_flexible_term(detail);
    assets_.upsert_flexible_term_detail(detail);
  }
  if (input.commercial_pension.has_value()) {
    CommercialPensionDetail detail = *input.commercial_pension;
    detail.asset_id = asset.id;
    normalize_commercial_pension(detail, std::nullopt);
    assets_.upsert_commercial_pension_detail(detail);
  }
  if (payment_source.has_value()) {
    // 期初金额仍是新资产的本金；付款只在来源资产记扣款流水。
    // 创建、明细、扣款及流水在同一事务里，失败时全部回滚。
    Transaction purchase;
    purchase.household_id = household_id;
    purchase.owner_member_id = payment_source->owner_member_id;
    purchase.type = TransactionType::Investment;
    purchase.action = InvestmentAction::Buy;
    purchase.transaction_time = asset.created_at;
    purchase.remark = "购入资产「" + asset.name + "」(#" + std::to_string(asset.id) + ")";
    purchase.status = TransactionStatus::Normal;
    purchase.created_at = asset.created_at;
    purchase.updated_at = asset.created_at;
    purchase.id = transactions_.create(purchase);
    const auto source_after = payment_source->current_balance - input.opening_balance;
    TransactionEntry source_entry;
    source_entry.transaction_id=purchase.id; source_entry.household_id=household_id;
    source_entry.owner_member_id=payment_source->owner_member_id; source_entry.asset_id=payment_source->id;
    source_entry.direction=TransactionDirection::Out; source_entry.amount=input.opening_balance;
    source_entry.balance_before=payment_source->current_balance; source_entry.balance_after=source_after;
    source_entry.created_at=asset.created_at; transactions_.add_entry(source_entry);
    TransactionEntry target_entry;
    target_entry.transaction_id=purchase.id; target_entry.household_id=household_id;
    target_entry.owner_member_id=asset.owner_member_id; target_entry.asset_id=asset.id;
    target_entry.direction=TransactionDirection::In; target_entry.amount=input.opening_balance;
    target_entry.balance_before=0; target_entry.balance_after=input.opening_balance;
    target_entry.created_at=asset.created_at; transactions_.add_entry(target_entry);
    transactions_.add_investment_detail(purchase.id,asset.id,InvestmentAction::Buy,input.opening_balance);
    assets_.update_balance(payment_source->id, source_after, asset.created_at);
    assets_.update_balance(asset.id, input.opening_balance, asset.created_at);
  }
  transaction.commit();

  return load_bundle(asset);
}

CreateMaintenancePreview AssetService::preview_create_maintenance(
    const AssetCreateInput& input) const {
  CreateMaintenancePreview preview;
  preview.asset_type = detail_type_of(input);
  const std::string today = time_util::business_today();

  if (input.bond_fund.has_value() &&
      input.bond_fund->holding_mode == HoldingMode::Rolling) {
    // 复用创建时的补全规则算出下一赎回日，再按每日维护规则判断是否需要推进。
    BondFundDetail detail = *input.bond_fund;
    normalize_bond_fund(detail, std::nullopt);
    if (detail.next_redeem_date.has_value()) {
      if (const auto advanced = maintenance_rules::advance_bond_fund_next(
              *detail.next_redeem_date, detail.holding_period_days, today);
          advanced.has_value()) {
        preview.changes.push_back(
            CreateMaintenanceChange{"next_redeem_date", *detail.next_redeem_date, *advanced});
      }
    }
  } else if (input.term_deposit.has_value() && input.term_deposit->auto_rollover) {
    TermDepositDetail detail = *input.term_deposit;
    normalize_term_deposit(detail);
    if (detail.maturity_date.has_value()) {
      if (const auto advanced = maintenance_rules::advance_term_period(
              *detail.maturity_date, *detail.term_value, *detail.term_unit, today);
          advanced.has_value()) {
        preview.changes.push_back(CreateMaintenanceChange{
            "start_date", detail.start_date.value_or(*detail.maturity_date), advanced->start});
        preview.changes.push_back(CreateMaintenanceChange{
            "maturity_date", *detail.maturity_date, advanced->maturity});
      }
    }
  }

  preview.required = !preview.changes.empty();
  return preview;
}

AssetBundle AssetService::get_bundle(std::int64_t id) {
  std::scoped_lock lock(database_.mutex());
  return load_bundle(get(id));
}

Asset AssetService::get(std::int64_t id) {
  std::scoped_lock lock(database_.mutex());
  const auto asset = assets_.find_by_id(id);
  if (!asset.has_value()) {
    throw not_found("asset not found");
  }
  return *asset;
}

std::vector<AssetBundle> AssetService::list_bundles(
    std::int64_t household_id, std::optional<std::int64_t> owner_member_id,
    std::optional<std::int64_t> account_id) {
  std::scoped_lock lock(database_.mutex());
  const auto assets = assets_.list_by_household(household_id, owner_member_id, account_id);
  std::vector<AssetBundle> bundles;
  bundles.reserve(assets.size());
  for (const auto& asset : assets) {
    bundles.push_back(load_bundle(asset));
  }
  return bundles;
}

AssetBundle AssetService::load_bundle(const Asset& asset) {
  // 明细表按 asset_type 1:0..1 关联，只有对应类型才去读明细，
  // 保证返回的 bundle 里至多一个明细块。
  AssetBundle bundle;
  bundle.asset = asset;
  switch (asset.asset_type) {
    case AssetType::TermDeposit:
      bundle.term_deposit = assets_.find_term_deposit_detail(asset.id);
      break;
    case AssetType::StockFund:
      bundle.stock_fund = assets_.find_stock_fund_detail(asset.id);
      break;
    case AssetType::BondFund:
      bundle.bond_fund = assets_.find_bond_fund_detail(asset.id);
      break;
    case AssetType::FlexibleTerm:
      bundle.flexible_term = assets_.find_flexible_term_detail(asset.id);
      break;
    case AssetType::CommercialPension:
      bundle.commercial_pension = assets_.find_commercial_pension_detail(asset.id);
      break;
    case AssetType::Insurance:
      bundle.insurance = assets_.find_insurance_detail(asset.id);
      break;
    default:
      break;
  }
  return bundle;
}

Asset AssetService::update_metadata(std::int64_t id, const std::string& name,
                                    std::optional<std::int64_t> opening_balance,
                                    const std::optional<std::string>& remark) {
  std::scoped_lock lock(database_.mutex());
  Asset asset = get(id);
  asset.name = strings::require_text(name, "name", 100);
  asset.remark = clean_optional(remark, "remark", 500);

  bool balance_changed = false;
  if (opening_balance.has_value() && *opening_balance != asset.opening_balance) {
    // 有流水后期初是不可追溯的历史锚点，改动会让 current_balance 与
    // 「opening_balance + Σdelta」不再自洽，故禁止；无流水时才允许修正。
    if (transactions_.count_by_asset(id) > 0) {
      throw conflict(
          "opening_balance cannot be changed after the asset has transactions");
    }
    validate_opening_balance(asset.asset_type, *opening_balance);
    // 无流水时 current_balance 等于 opening_balance，改期初即按差额同幅调整，
    // 维持 current_balance = opening_balance + Σdelta（此处 Σdelta 为 0）。
    const std::int64_t delta = *opening_balance - asset.opening_balance;
    asset.opening_balance = *opening_balance;
    asset.current_balance += delta;
    balance_changed = true;
  }

  asset.updated_at = time_util::now_iso8601();

  // 元数据与余额若都要改，放入同一事务，避免只改了其中一个。
  TransactionGuard transaction(database_);
  assets_.update_metadata(asset);
  if (balance_changed) {
    assets_.update_balance(asset.id, asset.current_balance, asset.updated_at);
  }
  transaction.commit();
  return asset;
}

Asset AssetService::set_current_balance(std::int64_t id, std::int64_t current_balance) {
  std::scoped_lock lock(database_.mutex());
  Asset asset = get(id);
  validate_opening_balance(asset.asset_type, current_balance);
  const std::int64_t delta = current_balance - asset.current_balance;
  asset.current_balance = current_balance;
  // 手工修改余额且不记流水时，将差额归入期初基准，维持 current_balance = opening_balance + Σdelta。
  asset.opening_balance += delta;
  asset.updated_at = time_util::now_iso8601();

  TransactionGuard transaction(database_);
  assets_.update_metadata(asset);
  assets_.update_balance(asset.id, asset.current_balance, asset.updated_at);
  transaction.commit();
  return asset;
}

Asset AssetService::update_status(std::int64_t id, AssetStatus status) {
  // 关闭（CLOSED）后资产不计入统计，并由交易服务拒绝新流水；此处仅改状态。
  std::scoped_lock lock(database_.mutex());
  Asset asset = get(id);
  asset.status = status;
  asset.updated_at = time_util::now_iso8601();
  assets_.update_status(asset.id, status, asset.updated_at);
  return asset;
}

AssetBundle AssetService::update_detail(std::int64_t id, AssetType detail_type,
                                        const TermDepositDetail* term_deposit,
                                        const StockFundDetail* stock_fund,
                                        const BondFundDetail* bond_fund,
                                        const FlexibleTermDetail* flexible_term,
                                        const CommercialPensionDetail* commercial_pension,
                                        const InsuranceDetail* insurance) {
  std::scoped_lock lock(database_.mutex());
  Asset asset = get(id);
  // 只能更新与资产自身类型相符的明细，防止明细与 asset_type 错配。
  if (detail_type != asset.asset_type) {
    throw invalid_request("detail type must match asset_type");
  }
  // upsert 语义：有则更新、无则插入，因此首次补充明细与后续修改共用此路径；
  // 对应类型的指针必须非空。Cash/Liability/Other 没有明细块。
  switch (detail_type) {
    case AssetType::TermDeposit: {
      if (term_deposit == nullptr) {
        throw invalid_request("term_deposit detail is required");
      }
      TermDepositDetail detail = *term_deposit;
      detail.asset_id = id;
      normalize_term_deposit(detail);
      assets_.upsert_term_deposit_detail(detail);
      break;
    }
    case AssetType::StockFund: {
      if (stock_fund == nullptr) {
        throw invalid_request("stock_fund detail is required");
      }
      StockFundDetail detail = *stock_fund;
      detail.asset_id = id;
      assets_.upsert_stock_fund_detail(detail);
      break;
    }
    case AssetType::BondFund: {
      if (bond_fund == nullptr) {
        throw invalid_request("bond_fund detail is required");
      }
      BondFundDetail detail = *bond_fund;
      detail.asset_id = id;
      // 编辑时若未显式给出 next_redeem_date，沿用数据库里已被每日维护推进的值，
      // 避免用户只改其它字段就把赎回日重置回首期。
      std::optional<std::string> existing_next;
      if (const auto existing = assets_.find_bond_fund_detail(id); existing.has_value()) {
        existing_next = existing->next_redeem_date;
      }
      normalize_bond_fund(detail, existing_next);
      assets_.upsert_bond_fund_detail(detail);
      break;
    }
    case AssetType::Insurance: {
      if (insurance == nullptr) {
        throw invalid_request("insurance detail is required");
      }
      InsuranceDetail detail = *insurance;
      detail.asset_id = id;
      assets_.upsert_insurance_detail(detail);
      break;
    }
    case AssetType::FlexibleTerm: {
      if (flexible_term == nullptr) throw invalid_request("flexible_term detail is required");
      FlexibleTermDetail detail = *flexible_term;
      detail.asset_id = id;
      normalize_flexible_term(detail);
      assets_.upsert_flexible_term_detail(detail);
      break;
    }
    case AssetType::CommercialPension: {
      if (commercial_pension == nullptr) {
        throw invalid_request("commercial_pension detail is required");
      }
      CommercialPensionDetail detail = *commercial_pension;
      detail.asset_id = id;
      normalize_commercial_pension(detail, assets_.find_commercial_pension_detail(id));
      assets_.upsert_commercial_pension_detail(detail);
      break;
    }
    default:
      throw invalid_request("this asset type has no detail block");
  }
  return load_bundle(asset);
}

void AssetService::remove(std::int64_t id) {
  std::scoped_lock lock(database_.mutex());
  get(id);  // 不存在则抛 not_found
  if (transactions_.count_by_asset(id) > 0) {
    throw conflict("assets with transaction entries cannot be deleted; close the asset instead");
  }
  // Entry 外键限制保留完整交易历史。
  TransactionGuard transaction(database_);
  assets_.remove(id);
  transaction.commit();
}

}  // namespace wt

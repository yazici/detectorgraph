// Copyright 2017 Nest Labs, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <memory>

#include "graph.hpp"
#include "detector.hpp"
#include "graphanalyzer.hpp"
#include "processorcontainer.hpp"
#include "lag.hpp"
#include "dglogging.hpp"

using namespace DetectorGraph;
using std::cout;
using std::endl;

/**
 * @file fancyvendingmachine.cpp
 * @brief Sophisticated Vending Machine example using a Lag-based feedback loop.
 *
 * @section ex-fvm-intro Introduction
 * You see, here things got a bit out of hand. All I was supposed to do was to
 * code a single representative and fun example. But then I got legitimately
 * nerd-snipped [1] and had to go all the way.
 *
 * @section ex-fvm-features Features
 * This example provides a vending machine algorithm that keeps track of:
 * - coin counting
 * - overlapping-balance purchases
 * - products in stock
 * - product prices
 * - canceling purchases half way through
 * - change giving (including a solution to the Change-Making problem [2] for
 * non-infinite sets of coins)
 * - dynamic product refill
 * - dynamic price updates
 * - financial report generation
 *
 * @section ex-fvm-lts Large TopicState
 * This example shows a concrete example of how to deal with
 * a Large TopicState - large enough that one wouldn't any unnecessary copies
 * of it. This is the case for the Look-Up Table generated by ChangeAlgo. That
 * table is conveyed inside the ChangeAvailable TopicState to allow for
 * efficient access to properties of that TopicState - namely to check whether
 * change for a given amount can be given. The way this is accomplished is by
 * using a shared_ptr to wrap the heap-allocated Look-Up Table.
 *
 * @section ex-fvm-arch Architecture
 * The graph uses 6 detectors and 14 TopicStates to encode the different logic and
 * data signals.
 *
 * The 'public' API to this graph is composed of:
 *  Inputs:
 *      - CoinInserted
 *      - MoneyBackButton
 *      - SelectedProduct
 *      - RefillProduct
 *      - RefillChange
 *      - PriceUpdate
 *  Outputs:
 *      - SaleProcessed
 *      - UserBalance
 *      - ReturnChange
 *      - FinancesReport
 *
 *
 * The graph below shows the relationships between the topics (rectangles) and
 * detectors (ellipses). Note that this graph can be automatically generated
 * for any instance of DetectorGraph::Graph using DetectorGraph::GraphAnalyzer.
 *  @dot "FancyVendingMachine"
digraph GraphAnalyzer {
    rankdir = "LR";
    node[fontname=Helvetica];
    size="12,5";

    "SelectedProduct" [label="0:SelectedProduct",style=filled, shape=box, color=lightblue];
        "SelectedProduct" -> "SaleProcessor";
    "MoneyBackButton" [label="1:MoneyBackButton",style=filled, shape=box, color=lightblue];
        "MoneyBackButton" -> "UserBalanceDetector";
    "CoinInserted" [label="2:CoinInserted",style=filled, shape=box, color=lightblue];
        "CoinInserted" -> "CoinBankManager";
        "CoinInserted" -> "UserBalanceDetector";
    "RefillChange" [label="3:RefillChange",style=filled, shape=box, color=lightblue];
        "RefillChange" -> "CoinBankManager";
    "PriceUpdate" [label="4:PriceUpdate",style=filled, shape=box, color=lightblue];
        "PriceUpdate" -> "ProductStockManager";
    "RefillProduct" [label="5:RefillProduct",style=filled, shape=box, color=lightblue];
        "RefillProduct" -> "ProductStockManager";
    "LaggedSaleProcessed" [label="6:Lagged<SaleProcessed>",style=filled, shape=box, color=lightblue];
        "LaggedSaleProcessed" -> "ProductStockManager";
        "LaggedSaleProcessed" -> "UserBalanceDetector";
        "LaggedSaleProcessed" -> "FinancesReportDetector";
    "UserBalanceDetector" [label="7:UserBalanceDetector", color=blue];
        "UserBalanceDetector" -> "UserBalance";
        "UserBalanceDetector" -> "ReturnChange";
    "ReturnChange" [label="8:ReturnChange",style=filled, shape=box, color=red];
        "ReturnChange" -> "CoinBankManager";
    "CoinBankManager" [label="9:CoinBankManager", color=blue];
        "CoinBankManager" -> "ReleaseCoins";
        "CoinBankManager" -> "ChangeAvailable";
    "ChangeAvailable" [label="10:ChangeAvailable",style=filled, shape=box, color=red];
        "ChangeAvailable" -> "SaleProcessor";
        "ChangeAvailable" -> "FinancesReportDetector";
    "ReleaseCoins" [label="11:ReleaseCoins",style=filled, shape=box, color=limegreen];
    "UserBalance" [label="12:UserBalance",style=filled, shape=box, color=red];
        "UserBalance" -> "SaleProcessor";
        "UserBalance" -> "FinancesReportDetector";
    "FinancesReportDetector" [label="13:FinancesReportDetector", color=blue];
        "FinancesReportDetector" -> "FinancesReport";
    "FinancesReport" [label="14:FinancesReport",style=filled, shape=box, color=limegreen];
    "ProductStockManager" [label="15:ProductStockManager", color=blue];
        "ProductStockManager" -> "StockState";
    "StockState" [label="16:StockState",style=filled, shape=box, color=red];
        "StockState" -> "SaleProcessor";
    "SaleProcessor" [label="17:SaleProcessor", color=blue];
        "SaleProcessor" -> "SaleProcessed";
    "SaleProcessed" [label="18:SaleProcessed",style=filled, shape=box, color=red];
        "SaleProcessed" -> "LagSaleProcessed";
    "LagSaleProcessed" [label="19:Lag<SaleProcessed>", color=blue];
        "LagSaleProcessed" -> "LaggedSaleProcessed" [style=dotted, color=red, constraint=false];
}
 *  @enddot
 *
 * @section ex-fvm-other-notes Other Notes
 * Note that this entire application in contained in a single file for the sake
 * of unity as an example. In real-world scenarios the suggested pattern is to
 * split the code into:
 *
 @verbatim
   detectorgraph/
        include/
            fancyvendingmachine.hpp (FancyVendingMachine header)
        src/
            fancyvendingmachine.hpp (FancyVendingMachine implementation)
        detectors/
            include/
                UserBalanceDetector.hpp
                SaleProcessor.hpp
                ProductStockManager.hpp
                CoinBankManager.hpp
                FinancesReportDetector.hpp
            src/
                UserBalanceDetector.cpp
                SaleProcessor.cpp
                ProductStockManager.cpp
                CoinBankManager.cpp
                FinancesReportDetector.cpp
        topicstates/
            include/
                CoinInserted.hpp
                SelectedProduct.hpp
                SaleProcessed.hpp
                RefillProduct.hpp
                PriceUpdate.hpp
                StockState.hpp
                UserBalance.hpp
                MoneyBackButton.hpp
                ReturnChange.hpp
                FinancesReport.hpp
                RefillChange.hpp
                ReleaseCoins.hpp
                ChangeAvailable.hpp
                CoinType.hpp (enum and GetStr - common core datatypes used
                              across multiple topicstates are also commonly
                              stored here)
                ProductIdType.hpp
            src/
                ChangeAvailable.cpp (TopicStates implementations, as needed)
                CoinType.cpp (e.g GetCoinTypeStr())
                ProductIdType.cpp
        utils/ (For utility modules/algos, as needed)
            include/
                ChangeAlgo.hpp
            src/
                ChangeAlgo.cpp

   Some projects that use Protocol Buffers for its TopicStates may have:
        proto/
            some_data.proto
@endverbatim
 *
 * Although this example makes heavy use of C++11, that is NOT a requirement
 * for DetectorGraph applications.
 *
 * @section ex-fvm-refs References
 *  - [1] Nerd Snipping - https://xkcd.com/356/
 *  - [2] Change-Making Problem - https://en.wikipedia.org/wiki/Change-making_problem
 *
 * @cond DO_NOT_DOCUMENT
 */
enum CoinType
{
    kCoinTypeNone = 0,
    kCoinType5c = 5,
    kCoinType10c = 10,
    kCoinType25c = 25,
    kCoinType50c = 50,
    kCoinType1d = 100,
};

enum ProductIdType{
    kProductIdTypeNone = 0,
    kSchokolade,
    kApfelzaft,
    kMate,
    kFrischMilch,
};

struct CoinInserted : public DetectorGraph::TopicState
{
    CoinType coin;
    CoinInserted(CoinType aCoin = kCoinTypeNone) : coin(aCoin) {}
};

struct SelectedProduct : public DetectorGraph::TopicState
{
    ProductIdType productId;
    SelectedProduct(ProductIdType id = kProductIdTypeNone) : productId(id) {}
};

struct SaleProcessed : public DetectorGraph::TopicState
{
    ProductIdType productId;
    int priceCents;
    SaleProcessed(ProductIdType aProduct = kProductIdTypeNone, int aPrice = 0) : productId(aProduct), priceCents(aPrice) {}
};

struct RefillProduct : public DetectorGraph::TopicState
{
    ProductIdType productId;
    int quantity;
    RefillProduct(ProductIdType aProduct = kProductIdTypeNone, int aQuantity = 0) : productId(aProduct), quantity(aQuantity) {}
};

//! [Mutually Atomic Variables]
struct PriceUpdate : public DetectorGraph::TopicState
{
    ProductIdType productId;
    int priceCents;
    PriceUpdate(ProductIdType aProduct = kProductIdTypeNone, int aPrice = 0) : productId(aProduct), priceCents(aPrice) {}
};
//! [Mutually Atomic Variables]

struct StockState : public DetectorGraph::TopicState
{
    struct ProductState
    {
        int count;
        int priceCents;
        ProductState(int aCount = 0, int aPrice = 0) : count(aCount), priceCents(aPrice) {}
    };

    std::map<ProductIdType, ProductState> products;
};

struct UserBalance : public DetectorGraph::TopicState
{
    int totalCents;
};

//![Trivial TopicState]
struct MoneyBackButton : public DetectorGraph::TopicState
{
};
//![Trivial TopicState]

struct ReturnChange : public DetectorGraph::TopicState
{
    int totalCents;
    ReturnChange(int total = 0) : totalCents(total) {}
};

struct FinancesReport : public DetectorGraph::TopicState
{
    FinancesReport(unsigned aBalance = 0) : balance(aBalance) {}
    unsigned balance;
};

namespace ChangeAlgo
{
    using CoinSet = std::vector<CoinType>;
    using CoinStock = std::map<CoinType, unsigned>;
    using Draw = std::map<CoinType, unsigned>;
    using LUTCell = std::vector<Draw>;
    using LUTRow = std::vector<LUTCell>;
    using LUTTable = std::vector<LUTRow>;

    class ChangeLookupTable
    {
    public:
        ChangeLookupTable(const ChangeAlgo::CoinSet& setOfCoins, unsigned maxChange)
        : mEmptyDraw(MakeEmptyDraw(setOfCoins))
        , mMaxChange(maxChange)
        , mSetOfCoins(setOfCoins)
        , mMinDenominator(*std::min_element(setOfCoins.begin(), setOfCoins.end()))
        {
            for (auto coin : mSetOfCoins)
            {
                assert(unsigned(coin) % mMinDenominator == 0);
            }
            assert(mMinDenominator != 0);
            assert(mMaxChange % mMinDenominator == 0);

            /*
            m = [[1, 2, 3,          ... max_target], (for )
                 [{c1:,c2:,c3:}, ... {c1:,c2:,c3:}], (after trying to add mSetOfCoins[0])
                 [{c1:,c2:,c3:}, ... {c1:,c2:,c3:}], (after trying to add mSetOfCoins[1])
                 ...
                 [{c1:,c2:,c3:}, ... {c1:,c2:,c3:}], (after trying to add mSetOfCoins[-1]) <---- Only this is necessary after the table is built.
                 ]

                 m[-1][x] is a list of sets of coins that produce X
            */
            unsigned midTargets = unsigned(mMaxChange / mMinDenominator);
            unsigned targetColumnsRange = midTargets + 1;

            ChangeAlgo::LUTTable lutt = ChangeAlgo::LUTTable(mSetOfCoins.size(), ChangeAlgo::LUTRow(targetColumnsRange));

            for (unsigned coinIndex = 0; coinIndex < lutt.size(); ++coinIndex)
            {
                CoinType denominationType = mSetOfCoins[coinIndex];
                unsigned denomination = static_cast<unsigned>(denominationType);
                unsigned denominationSteps = unsigned(denomination / mMinDenominator);
                // cout << "coinIndex=" << coinIndex << endl;
                for (unsigned targetIdx = 0; targetIdx < targetColumnsRange; ++targetIdx)
                {
                    unsigned target = targetIdx * mMinDenominator;

                    // Use just this coin
                    if (denomination == target)
                    {
                        // Create new draw from a single coin
                        lutt[coinIndex][targetIdx].push_back(IncrementedDraw(mEmptyDraw, denominationType));
                    }
                    // Don't use this coin
                    else if (denomination > target)
                    {
                        if (coinIndex > 0)
                        {
                            // Copy from previous coin's solution (Up)
                            ChangeAlgo::LUTCell& fromCell = lutt[coinIndex - 1][targetIdx];
                            ChangeAlgo::LUTCell& toCell = lutt[coinIndex][targetIdx];
                            std::copy(fromCell.begin(), fromCell.end(), std::back_inserter(toCell)); // If we're not touching, why copying?
                        }
                    }
                    // Try adding two sets (using and not using this coin)
                    else
                    {
                        if (coinIndex > 0)
                        {
                            // Copy from previous coin's solution (Up)
                            ChangeAlgo::LUTCell& fromCell = lutt[coinIndex - 1][targetIdx];
                            ChangeAlgo::LUTCell& toCell = lutt[coinIndex][targetIdx];
                            std::copy(fromCell.begin(), fromCell.end(), std::back_inserter(toCell)); // If we're not touching, why copying?
                        }

                        // Create new draws from incrementing solutions to smaller targets (Left)
                        for (ChangeAlgo::Draw leftDraw : lutt[coinIndex][targetIdx - denominationSteps])
                        {
                            lutt[coinIndex][targetIdx].push_back(IncrementedDraw(leftDraw, denominationType));
                        }
                    }
                }
            }
            // lutt.back() the last LUTRow ot LUTTable containing the results considering all coins.
            mLookupRow = lutt.back();
        }

        Draw IncrementedDraw(const Draw& emptyDraw, CoinType denominationType) const
        {
            Draw draw = emptyDraw;
            draw[denominationType] += 1;
            return draw;
        }

        Draw MakeEmptyDraw(const CoinSet& coinSet) const
        {
            Draw emptyDraw;
            for (auto coinType : coinSet)
            {
                emptyDraw[coinType] = 0;
            }
            return emptyDraw;
        }

        const LUTCell& GetChangeDraws(unsigned change) const
        {
            assert(mMinDenominator != 0);
            assert(change % mMinDenominator == 0);
            assert(change <= mMaxChange);

            unsigned t_idx = unsigned(change / mMinDenominator);
            return mLookupRow.at(t_idx);
        }

        unsigned GetDrawScore(const Draw& draw) const
        {
            return std::accumulate(draw.begin(), draw.end(), 0,
                [](unsigned accumulator, const Draw::value_type& value) { return accumulator + value.second; });
        }

        bool IsDrawPossible(const CoinStock& availableCoins, const Draw& draw) const
        {
            return std::all_of(availableCoins.begin(), availableCoins.end(), [&draw](const CoinStock::value_type& availableCoin) {
                return (draw.at(availableCoin.first) <= availableCoin.second); // Draw has less-eq coins than available
            });
        }

        Draw GetSmallestChange(unsigned change) const
        {
            const LUTCell& draws = GetChangeDraws(change);
            return *std::min_element(draws.begin(), draws.end(), [this](const Draw& d1, const Draw& d2) {
                return GetDrawScore(d1) < GetDrawScore(d2);  // Draw d1 has less total coins than d2
            });
        }

        Draw GetSmallestChange(const CoinStock& availableCoins, unsigned change) const
        {
            const LUTCell& draws = GetChangeDraws(change);
            LUTCell possibleDraws;
            std::copy_if(draws.begin(), draws.end(), std::back_inserter(possibleDraws), [this, &availableCoins](const Draw& d) {
                return IsDrawPossible(availableCoins, d);
            });

            return *std::min_element(possibleDraws.begin(), possibleDraws.end(), [this](const Draw& d1, const Draw& d2) {
                return GetDrawScore(d1) < GetDrawScore(d2);  // Draw d1 has less total coins than d2
            });
        }

        bool CanGiveChange(const CoinStock& availableCoins, unsigned change) const
        {
            const LUTCell& draws = GetChangeDraws(change);
            return (bool)std::count_if(draws.begin(), draws.end(), [this, &availableCoins](const Draw& d) {
                return IsDrawPossible(availableCoins, d);
            });
        }

    private:
        /* const */ Draw mEmptyDraw;
        /* const */ unsigned mMaxChange;
        /* const */ CoinSet mSetOfCoins;
        /* const */ unsigned mMinDenominator;
        LUTRow mLookupRow;
    };
}

//! [TopicStates Inheritance Example]
struct RefillChange : public DetectorGraph::TopicState, public ChangeAlgo::CoinStock
{
    RefillChange() {}
    RefillChange(const ChangeAlgo::CoinStock& stock) : ChangeAlgo::CoinStock(stock) {}
};

struct ReleaseCoins : public DetectorGraph::TopicState, public ChangeAlgo::Draw
{
    ReleaseCoins() {}
    ReleaseCoins(const ChangeAlgo::Draw& draw) : ChangeAlgo::Draw(draw) {}
};
//! [TopicStates Inheritance Example]

//! [Immutable Shared Memory TopicState]
struct ChangeAvailable : public DetectorGraph::TopicState
{
    ChangeAvailable() : mCoins(), mpChangeLookupTable() {}

    ChangeAvailable(const ChangeAlgo::CoinStock& aCoins, const std::shared_ptr<const ChangeAlgo::ChangeLookupTable>& aLookupTable)
    : mCoins(aCoins)
    , mpChangeLookupTable(aLookupTable)
    {
    }

    bool CanGiveChange(unsigned change) const
    {
        return mpChangeLookupTable->CanGiveChange(mCoins, change);
    }

    ChangeAlgo::CoinStock mCoins;
    std::shared_ptr<const ChangeAlgo::ChangeLookupTable> mpChangeLookupTable;
};
//! [Immutable Shared Memory TopicState]

class UserBalanceDetector : public DetectorGraph::Detector
, public DetectorGraph::SubscriberInterface< Lagged<SaleProcessed> >
, public DetectorGraph::SubscriberInterface<CoinInserted>
, public DetectorGraph::SubscriberInterface<MoneyBackButton>
, public DetectorGraph::Publisher<UserBalance>
, public DetectorGraph::Publisher<ReturnChange>
{
public:
    UserBalanceDetector(DetectorGraph::Graph* graph) : DetectorGraph::Detector(graph), mUserBalance()
    {
        Subscribe< Lagged<SaleProcessed> >(this);
        Subscribe<CoinInserted>(this);
        Subscribe<MoneyBackButton>(this);
        SetupPublishing<UserBalance>(this);
        SetupPublishing<ReturnChange>(this);
    }

    virtual void Evaluate(const Lagged<SaleProcessed>& sale)
    {
        mUserBalance.totalCents -= sale.data.priceCents;
    }

    virtual void Evaluate(const CoinInserted& inserted)
    {
        mUserBalance.totalCents += inserted.coin;
    }

    virtual void Evaluate(const MoneyBackButton&)
    {
        Publisher<ReturnChange>::Publish(mUserBalance.totalCents);
        mUserBalance.totalCents = 0;
    }

    virtual void CompleteEvaluation()
    {
        DG_ASSERT(mUserBalance.totalCents >= 0);
        DG_LOG("UserBalance total = %d cents", mUserBalance.totalCents);
        Publisher<UserBalance>::Publish(mUserBalance);
    }

private:
    UserBalance mUserBalance;

};

class SaleProcessor : public DetectorGraph::Detector
, public DetectorGraph::SubscriberInterface<UserBalance>
, public DetectorGraph::SubscriberInterface<SelectedProduct>
, public DetectorGraph::SubscriberInterface<StockState>
, public DetectorGraph::SubscriberInterface<ChangeAvailable>
, public DetectorGraph::Publisher<SaleProcessed>
{
public:
    SaleProcessor(DetectorGraph::Graph* graph) : DetectorGraph::Detector(graph)
    {
        Subscribe<UserBalance>(this);
        Subscribe<SelectedProduct>(this);
        Subscribe<StockState>(this);
        Subscribe<ChangeAvailable>(this);
        SetupPublishing<SaleProcessed>(this);
    }

    virtual void Evaluate(const UserBalance& aUserBalance)
    {
        mUserBalance = aUserBalance;
    }

    virtual void Evaluate(const SelectedProduct& aSelection)
    {
        mSelection = aSelection;
        DG_LOG("SaleProcessor; Selected ProductId=%d", mSelection.productId);
    }

    virtual void Evaluate(const StockState& aStock)
    {
        mStock = aStock;
    }

    virtual void Evaluate(const ChangeAvailable& aChangeAvailable)
    {
        mChangeAvailable = aChangeAvailable;
    }

    virtual void CompleteEvaluation()
    {
        const auto stockForProduct = mStock.products.find(mSelection.productId);
        if (stockForProduct != mStock.products.end() &&
            stockForProduct->second.count > 0 &&
            stockForProduct->second.priceCents <= mUserBalance.totalCents &&
            mChangeAvailable.CanGiveChange(mUserBalance.totalCents - stockForProduct->second.priceCents))
        {
            Publish(SaleProcessed(mSelection.productId, stockForProduct->second.priceCents));
        }
    }

private:
    UserBalance mUserBalance;
    SelectedProduct mSelection;
    StockState mStock;
    ChangeAvailable mChangeAvailable;

};

class ProductStockManager : public DetectorGraph::Detector
, public DetectorGraph::SubscriberInterface< Lagged<SaleProcessed> >
, public DetectorGraph::SubscriberInterface<RefillProduct>
, public DetectorGraph::SubscriberInterface<PriceUpdate>
, public DetectorGraph::Publisher<StockState>
{
public:
    ProductStockManager(DetectorGraph::Graph* graph) : DetectorGraph::Detector(graph)
    {
        Subscribe< Lagged<SaleProcessed> >(this);
        Subscribe<RefillProduct>(this);
        Subscribe<PriceUpdate>(this);
        SetupPublishing<StockState>(this);
    }

    virtual void Evaluate(const Lagged<SaleProcessed>& aSale)
    {
        //remove from mStock
        mStock.products[aSale.data.productId].count--;
        DG_ASSERT(mStock.products[aSale.data.productId].count >= 0);
    }

    virtual void Evaluate(const RefillProduct& aRefill)
    {
        // add to mStock
        mStock.products[aRefill.productId].count += aRefill.quantity;
    }

    virtual void Evaluate(const PriceUpdate& aUpdate)
    {
        // update product price
        mStock.products[aUpdate.productId].priceCents = aUpdate.priceCents;
    }

    virtual void CompleteEvaluation()
    {
        // publish mStock
        Publish(mStock);
    }

private:
    StockState mStock;

};

class CoinBankManager : public DetectorGraph::Detector
, public DetectorGraph::SubscriberInterface<ReturnChange>
, public DetectorGraph::SubscriberInterface<RefillChange>
, public DetectorGraph::SubscriberInterface<CoinInserted>
, public DetectorGraph::Publisher<ReleaseCoins>
, public DetectorGraph::Publisher<ChangeAvailable>
{
public:
    CoinBankManager(DetectorGraph::Graph* graph)
    : DetectorGraph::Detector(graph)
    , mAvailable()
    , mpChangeLookupTable(
        std::make_shared<ChangeAlgo::ChangeLookupTable>(
            ChangeAlgo::CoinSet({kCoinType5c, kCoinType10c, kCoinType25c, kCoinType50c, kCoinType1d}), 300))
    {
        Subscribe<ReturnChange>(this);
        Subscribe<RefillChange>(this);
        Subscribe<CoinInserted>(this);
        SetupPublishing<ReleaseCoins>(this);
        SetupPublishing<ChangeAvailable>(this);

        // ComputeLookup({kCoinType25c, kCoinType50c}, 200);
    }

    virtual void Evaluate(const ReturnChange& aChange)
    {
        // remove from stock according to fancy proprietary change-giving
        // algorithm. Ha! Ha, Ha..
        ChangeAlgo::Draw returningChange = mpChangeLookupTable->GetSmallestChange(mAvailable, aChange.totalCents);

        Publisher<ReleaseCoins>::Publish(ReleaseCoins(returningChange));
    }

    virtual void Evaluate(const RefillChange& aRefill)
    {
        // add refill to stock
        for (auto coinRefill : aRefill)
        {
            mAvailable[coinRefill.first] += coinRefill.second;
        }
    }

    virtual void Evaluate(const CoinInserted& aInserted)
    {
        // add inserted coins to stock
        mAvailable[aInserted.coin]++;
    }

    virtual void CompleteEvaluation()
    {
        Publisher<ChangeAvailable>::Publish(ChangeAvailable(mAvailable, mpChangeLookupTable));
    }

private:
    ChangeAlgo::CoinStock mAvailable;
    std::shared_ptr<const ChangeAlgo::ChangeLookupTable> mpChangeLookupTable;
};

class FinancesReportDetector : public DetectorGraph::Detector
, public DetectorGraph::SubscriberInterface<ChangeAvailable>
, public DetectorGraph::SubscriberInterface<UserBalance>
, public DetectorGraph::SubscriberInterface< Lagged<SaleProcessed> >
, public DetectorGraph::Publisher<FinancesReport>
{
public:
    FinancesReportDetector(DetectorGraph::Graph* graph)
    : DetectorGraph::Detector(graph)
    , mChangeAvailable()
    {
        Subscribe<ChangeAvailable>(this);
        Subscribe<UserBalance>(this);
        Subscribe< Lagged<SaleProcessed> >(this);
        SetupPublishing<FinancesReport>(this);
    }

    virtual void Evaluate(const ChangeAvailable& changeAvailable)
    {
        mChangeAvailable = changeAvailable;
    }

    virtual void Evaluate(const UserBalance& userBalance)
    {
        mUserBalance = userBalance;
    }

    virtual void Evaluate(const Lagged<SaleProcessed>&)
    {
        const ChangeAlgo::CoinStock& stock = mChangeAvailable.mCoins;
        unsigned coinsBalance = std::accumulate(stock.begin(), stock.end(), 0,
                [](unsigned accumulator, const ChangeAlgo::CoinStock::value_type& value) {
                    return accumulator + (value.first * value.second);
                });

        Publish(FinancesReport(coinsBalance - mUserBalance.totalCents));
    }

private:
    ChangeAvailable mChangeAvailable;
    UserBalance mUserBalance;
};

const char * GetCoinTypeStr(CoinType c)
{
    switch(c)
    {
        case kCoinType5c: return "5c";
        case kCoinType10c: return "10c";
        case kCoinType25c: return "25c";
        case kCoinType50c: return "50c";
        case kCoinType1d: return "1d";
        case kCoinTypeNone: return "NOT A COIN";
    }
    return "INVALID ENUM VALUE";
}

const char * GetProductIdStr(ProductIdType p)
{
    switch(p)
    {
        case kSchokolade: return "Schokolade";
        case kApfelzaft: return "Apfelzaft";
        case kMate: return "Mate";
        case kFrischMilch: return "FrischMilch";
        case kProductIdTypeNone: return "NOT A PRODUCT";
    }
    return "INVALID ENUM VALUE";
}

class FancyVendingMachine : public DetectorGraph::ProcessorContainer
{
public:
    FancyVendingMachine()
    : mProductStockManager(&mGraph)
    , mCoinBankManager(&mGraph)
    , mUserBalanceDetector(&mGraph)
    , mSaleProcessor(&mGraph)
    , mSaleFeedBack(&mGraph)
    , mFinancesReportDetector(&mGraph)
    , saleTopic(mGraph.ResolveTopic<SaleProcessed>())
    , changeReleaseTopic(mGraph.ResolveTopic<ReleaseCoins>())
    , financeReportTopic(mGraph.ResolveTopic<FinancesReport>())
    {
    }

    ProductStockManager mProductStockManager;
    CoinBankManager mCoinBankManager;
    UserBalanceDetector mUserBalanceDetector;
    SaleProcessor mSaleProcessor;
    Lag<SaleProcessed> mSaleFeedBack;
    FinancesReportDetector mFinancesReportDetector;
    Topic<SaleProcessed>* saleTopic;
    Topic<ReleaseCoins>* changeReleaseTopic;
    Topic<FinancesReport>* financeReportTopic;

    virtual void ProcessOutput()
    {
        if (saleTopic->HasNewValue())
        {
            const auto sale = saleTopic->GetNewValue();
            cout << "Sold " << GetProductIdStr(sale.productId) << " for " << sale.priceCents << endl;
        }
        if (changeReleaseTopic->HasNewValue())
        {
            const auto changeReleased = changeReleaseTopic->GetNewValue();
            cout << "Money Returned ";
            for (auto coin : changeReleased)
            {
                cout << coin.second << "x" << GetCoinTypeStr(coin.first) << ", ";
            }
            cout << endl;
        }
        if (financeReportTopic->HasNewValue())
        {
            const auto report = financeReportTopic->GetNewValue();
            cout << "Current Balance: " << report.balance << endl;
        }
    }
};

int main()
{
    FancyVendingMachine fancyVendingMachine;

    fancyVendingMachine.ProcessData(RefillChange({{kCoinType25c, 0},
                                                  {kCoinType50c, 0}}));

    fancyVendingMachine.ProcessData(PriceUpdate(kFrischMilch, 200));
    fancyVendingMachine.ProcessData(PriceUpdate(kSchokolade, 100));
    fancyVendingMachine.ProcessData(PriceUpdate(kApfelzaft, 150));
    fancyVendingMachine.ProcessData(RefillProduct(kFrischMilch, 5));
    fancyVendingMachine.ProcessData(RefillProduct(kSchokolade, 4));
    fancyVendingMachine.ProcessData(RefillProduct(kApfelzaft, 3));

    fancyVendingMachine.ProcessData(CoinInserted(kCoinType25c));
    fancyVendingMachine.ProcessData(CoinInserted(kCoinType50c));
    fancyVendingMachine.ProcessData(CoinInserted(kCoinType50c));
    fancyVendingMachine.ProcessData(CoinInserted(kCoinType50c));
    fancyVendingMachine.ProcessData(SelectedProduct(kApfelzaft));

    fancyVendingMachine.ProcessData(MoneyBackButton());

    fancyVendingMachine.ProcessData(CoinInserted(kCoinType25c));
    fancyVendingMachine.ProcessData(CoinInserted(kCoinType50c));
    fancyVendingMachine.ProcessData(CoinInserted(kCoinType50c));

    fancyVendingMachine.ProcessData(SelectedProduct(kApfelzaft));

    fancyVendingMachine.ProcessData(MoneyBackButton());

    GraphAnalyzer analyzer(fancyVendingMachine.mGraph);
    analyzer.GenerateDotFile("fancy_vending_machine.dot");
}

/// @endcond DO_NOT_DOCUMENT

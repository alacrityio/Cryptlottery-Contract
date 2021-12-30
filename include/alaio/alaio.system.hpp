#pragma once

#include <alaio/asset.hpp>
#include <alaio/privileged.hpp>
#include <alaio/singleton.hpp>
#include <alaio/system.hpp>
#include <alaio/time.hpp>

#include <alaio.system/exchange_state.hpp>
#include <alaio.system/native.hpp>

#include <deque>
#include <optional>
#include <string>
#include <type_traits>


#ifdef CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX
#undef CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX
#endif
// CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX macro determines whether ramfee and namebid proceeds are
// channeled to REX pool. In order to stop these proceeds from being channeled, the macro must
// be set to 0.
#define CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX 1

namespace alaiosystem {

   using alaio::asset;
   using alaio::block_timestamp;
   using alaio::check;
   using alaio::const_mem_fun;
   using alaio::datastream;
   using alaio::indexed_by;
   using alaio::name;
   using alaio::same_payer;
   using alaio::symbol;
   using alaio::symbol_code;
   using alaio::time_point;
   using alaio::time_point_sec;
   using alaio::unsigned_int;
   using alaio::microseconds;
   using alaio::print;
   using alaio::current_time_point;
   using alaio::current_block_time;

   template<typename E, typename F>
   static inline auto has_field( F flags, E field )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, bool>
   {
      return ( (flags & static_cast<F>(field)) != 0 );
   }

   template<typename E, typename F>
   static inline auto set_field( F flags, E field, bool value = true )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, F >
   {
      if( value )
         return ( flags | static_cast<F>(field) );
      else
         return ( flags & ~static_cast<F>(field) );
   }

   static constexpr uint32_t seconds_per_year      = 52 * 7 * 24 * 3600;
   static constexpr uint32_t seconds_per_day       = 24 * 3600;
   static constexpr int64_t  useconds_per_year     = int64_t(seconds_per_year) * 1000'000ll;
   static constexpr int64_t  useconds_per_day      = int64_t(seconds_per_day) * 1000'000ll;
   static constexpr uint32_t blocks_per_day        = 2 * seconds_per_day; // half seconds per day

   static constexpr int64_t  min_pervote_daily_pay = 1'0000;
   static constexpr int64_t  min_activated_stake   = 1'000'0000;
   
   static constexpr int64_t  ram_gift_bytes        = 1400;   
   static constexpr double   continuous_rate       = 0.04879;          // 5% annual rate
   static constexpr int64_t  inflation_pay_factor  = 5;                // 20% of the inflation
   static constexpr int64_t  votepay_factor        = 4;                // 25% of the producer pay
   static constexpr uint32_t refund_delay_sec      = 3 * seconds_per_day;


   /**
    *
    * @defgroup alaiosystem alaio.system
    * @ingroup alaiocontracts
    * alaio.system contract defines the structures and actions needed for blockchain's core functionality.
    * - Users can stake tokens for CPU and Network bandwidth, and then vote for producers or
    *    delegate their vote to a proxy.
    * - Producers register in order to be voted for, and can claim per-block and per-vote rewards.
    * - Users can buy and sell RAM at a market-determined price.
    * - Users can bid on premium names.
    * - A resource exchange system (REX) allows token holders to lend their tokens,
    *    and users to rent CPU and Network resources in return for a market-determined fee.
    * @{
    */

   /**
    * A name bid.
    *
    * @details A name bid consists of:
    * - a `newname` name that the bid is for
    * - a `high_bidder` account name that is the one with the highest bid so far
    * - the `high_bid` which is amount of highest bid
    * - and `last_bid_time` which is the time of the highest bid
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] name_bid {
     name            newname;
     name            high_bidder;
     int64_t         high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
     time_point      last_bid_time;

     uint64_t primary_key()const { return newname.value;                    }
     uint64_t by_high_bid()const { return static_cast<uint64_t>(-high_bid); }
   };

   /**
    * A bid refund.
    *
    * @details A bid refund is defined by:
    * - the `bidder` account name owning the refund
    * - the `amount` to be refunded
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] bid_refund {
      name         bidder;
      asset        amount;

      uint64_t primary_key()const { return bidder.value; }
   };

   /**
    * Name bid table
    *
    * @details The name bid table is storing all the `name_bid`s instances.
    */
   typedef alaio::multi_index< "namebids"_n, name_bid,
                               indexed_by<"highbid"_n, const_mem_fun<name_bid, uint64_t, &name_bid::by_high_bid>  >
                             > name_bid_table;

   /**
    * Bid refund table.
    *
    * @details The bid refund table is storing all the `bid_refund`s instances.
    */
   typedef alaio::multi_index< "bidrefunds"_n, bid_refund > bid_refund_table;

   /**
    * Defines new global state parameters.
    */
   /*struct [[alaio::table("global"), alaio::contract("alaio.system")]] alaio_global_state : alaio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      time_point           last_pervote_bucket_fill;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      time_point           thresh_activated_stake_time;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE_DERIVED( alaio_global_state, alaio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };*/

   // Setting a inflated value strcuture
//   struct [[alaio::table("global"), alaio::contract("alaio.system")]] alaio_global_state : alaio::blockchain_parameters {
//      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

 //     uint64_t             max_ram_size = 64ll * 1024 * 1024 * 1024;
 //     uint64_t             total_ram_bytes_reserved = 0;
   //   int64_t              total_ram_stake = 0;

     // block_timestamp      last_producer_schedule_update;
     // time_point           last_pervote_bucket_fill;
     // time_point           last_dapp_bucket_fill;
     // time_point           last_perblock_bucket_fill;
      //int64_t              perblock_bucket = 0;
     // int64_t              pervote_bucket = 0;
     // int64_t              perwitness_bucket = 0;
     // int64_t              dapps_per_transfer_rewards_bucket = 0;
     // int64_t              dapps_per_user_rewards_bucket = 0;
     // uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
     // int64_t              total_unpaid_votes = 0; /// all votes that users made but not paid
     // uint32_t             oracle_bucket = 0;
     // int64_t              total_activated_stake = 0;
     // time_point           thresh_activated_stake_time;
     // uint16_t             last_producer_schedule_size = 0;
     // double               total_producer_vote_weight = 0; /// the sum of all producer votes
     // block_timestamp      last_name_close;

      // user voting power is upper-limited to 1000 tokens, no matter how much is staked
     // int64_t              max_vote_power = 1'000'0000;
     // microseconds         vote_claim_period = alaio::days( 30 );

      // explicit serialization macro is not necessary, used here only to improve compilation time
      //ALALIB_SERIALIZE_DERIVED( alaio_global_state, alaio::blockchain_parameters,
       //                         (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
       //                         (last_producer_schedule_update)(last_pervote_bucket_fill)(last_dapp_bucket_fill)(last_perblock_bucket_fill)
       //                         (perblock_bucket)(pervote_bucket)(perwitness_bucket)(dapps_per_transfer_rewards_bucket)(dapps_per_user_rewards_bucket)
        //                        (total_unpaid_blocks)(total_unpaid_votes)(oracle_bucket)(total_activated_stake)(thresh_activated_stake_time)
          //                      (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close)(max_vote_power)(vote_claim_period) )
  // };

   // Setting a constant value structure
    struct [[alaio::table("global"), alaio::contract("alaio.system")]] alaio_global_state : alaio::blockchain_parameters {
       uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

       uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
       uint64_t             total_ram_bytes_reserved = 0;
       int64_t              total_ram_stake = 0;
       // Setting the timepoint for all the buckets
       time_point           last_perblock_bucket_fill;
       time_point           last_pervote_bucket_fill;
       time_point           last_dapp_bucket_fill;
       time_point           last_perwitness_bucket_fill;
       // Setting up the dynamic variables
       block_timestamp      last_producer_schedule_update;
       uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
       int64_t              total_unpaid_votes = 0; /// all votes that users made but not paid
       int64_t              total_activated_stake = 0;
       time_point           thresh_activated_stake_time;
       uint16_t             last_producer_schedule_size = 0;
       double               total_producer_vote_weight = 0; /// the sum of all producer votes
       block_timestamp      last_name_close;
       int64_t              supply = 800'000'000'0000;
       int32_t              inflation_rate = 5'0000;
       int64_t              inflated_amount = 40'000'000'0000;
       time_point           last_inflation_rate_update;
       // Setting up the constant variables
       uint64_t             peryear_block_bucket = 12'000'000'0000;
       uint64_t             peryear_community_block_bucket = 5'720'000'0000;
       uint64_t             peryear_witness_bucket = 3'000'000'0000;
       uint64_t             peryear_dapps_per_transfer_rewards_bucket = 2'000'000'0000;
       uint64_t             peryear_dapps_per_user_rewards_bucket = 2'000'000'0000;
       uint64_t             peryear_pervote_bucket = 1'000'000'0000;
       uint64_t             peryear_community_team_bucket = 7'020'000'0000;
       uint64_t             peryear_founding_team_bucket = 5'000'000'0000;
       uint64_t             peryear_marketing_team_bucket = 2'000'000'0000;
       uint64_t             peryear_oracle_reward_bucket = 260'000'0000;
       uint64_t             daily_block_bucket = 0;
       uint64_t             daily_witness_bucket = 0;
       uint64_t             daily_oracle_reward_bucket = 0;
       uint64_t             monthly_dapps_per_transfer_rewards_bucket = 0;
       uint64_t             monthly_dapps_per_user_rewards_bucket = 0;
       uint64_t             monthly_pervote_bucket = 0;
       // user voting power is upper-limited to 1000 tokens, no matter how much is staked
       int64_t              max_vote_power = 1'000'0000;
       microseconds         vote_claim_period = alaio::days( 30 );

       // explicit serialization macro is not necessary, used here only to improve compilation time
       ALALIB_SERIALIZE_DERIVED( alaio_global_state, alaio::blockchain_parameters, 
                               (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                               (last_perblock_bucket_fill)(last_pervote_bucket_fill)(last_dapp_bucket_fill)
                               (last_perwitness_bucket_fill)(last_producer_schedule_update)(total_unpaid_blocks)
                               (total_unpaid_votes)(total_activated_stake)(thresh_activated_stake_time)
                               (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close)(supply)
                               (inflation_rate)(inflated_amount)(last_inflation_rate_update)(peryear_block_bucket)
                               (peryear_community_block_bucket)(peryear_witness_bucket)(peryear_dapps_per_transfer_rewards_bucket)
                               (peryear_dapps_per_user_rewards_bucket)(peryear_pervote_bucket)(peryear_community_team_bucket)
                               (peryear_founding_team_bucket)(peryear_marketing_team_bucket)(peryear_oracle_reward_bucket)
                               (daily_block_bucket)(daily_witness_bucket)(daily_oracle_reward_bucket)
                               (monthly_dapps_per_transfer_rewards_bucket)(monthly_dapps_per_user_rewards_bucket)
                               (monthly_pervote_bucket)(max_vote_power)(vote_claim_period) )
    };

   /**
    * Defines new global state parameters added after version 1.0
    */
   struct [[alaio::table("global2"), alaio::contract("alaio.system")]] alaio_global_state2 {
      alaio_global_state2(){}

      uint16_t          new_ram_per_block = 0;
      block_timestamp   last_ram_increase;
      block_timestamp   last_block_num; /* deprecated */
      double            total_producer_votepay_share = 0;
      uint8_t           revision = 0; ///< used to track version updates in the future.

      ALALIB_SERIALIZE( alaio_global_state2, (new_ram_per_block)(last_ram_increase)(last_block_num)
                        (total_producer_votepay_share)(revision) )
   };

   /**
    * Defines new global state parameters added after version 1.3.0
    */
   struct [[alaio::table("global3"), alaio::contract("alaio.system")]] alaio_global_state3 {
      alaio_global_state3() { }
      time_point        last_vpay_state_update;
      double            total_vpay_share_change_rate = 0;

      ALALIB_SERIALIZE( alaio_global_state3, (last_vpay_state_update)(total_vpay_share_change_rate) )
   };

   struct [[alaio::table("global4"), alaio::contract("alaio.system")]] alaio_global_state4 {
      alaio_global_state4() { }
      time_point        last_wpay_state_update;
      double            total_wpay_share_change_rate = 0;
      double            total_producer_witnesspay_share = 0;

      ALALIB_SERIALIZE( alaio_global_state4, (last_wpay_state_update)(total_wpay_share_change_rate)(total_producer_witnesspay_share) )
   };

   struct [[alaio::table("global5"), alaio::contract("alaio.system")]] alaio_global_state5 {
      alaio_global_state5() { }
      uint64_t    active_users = 0;
      uint64_t    temp_active_users = 0;
      uint64_t    last_name_value = 0;
      time_point  date_updated;
      bool        updating = false;

      ALALIB_SERIALIZE( alaio_global_state5, (active_users)(temp_active_users)(last_name_value)(date_updated)(updating) )
   };

   struct [[alaio::table("globalbps"), alaio::contract("alaio.system")]] alaio_global_state_bps {
      alaio_global_state_bps() { }
      uint64_t    last_name_value = 0;
      bool        updating = false;
      time_point  date_reset;

      ALALIB_SERIALIZE( alaio_global_state_bps, (last_name_value)(updating)(date_reset) )
   };

   /**
    * Defines `producer_info` structure to be stored in `producer_info` table, added after version 1.0
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] producer_info {
      name                  owner;
      double                total_votes = 0;
      alaio::public_key     producer_key; /// a packed public key object
      bool                  is_active = true;
      std::string           url;
      uint32_t              unpaid_blocks = 0;
      time_point            last_claim_time;
      uint16_t              location = 0;

      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); is_active = false; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( producer_info, (owner)(total_votes)(producer_key)(is_active)(url)
                        (unpaid_blocks)(last_claim_time)(location) )
   };

   /**
    * Defines new producer info structure to be stored in new producer info table, added after version 1.3.0
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] producer_info2 {
      name            owner;
      double          votepay_share = 0;
      time_point      last_votepay_share_update;

      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( producer_info2, (owner)(votepay_share)(last_votepay_share_update) )
   };

   struct [[alaio::table, alaio::contract("alaio.system")]] producer_info3 {
      name            owner;
      double          witnesspay_share = 0;
      time_point      last_witnesspay_share_update;

      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( producer_info3, (owner)(witnesspay_share)(last_witnesspay_share_update) )
   };

   /**
    * Voter info.
    *
    * @details Voter info stores information about the voter:
    * - `owner` the voter
    * - `proxy` the proxy set by the voter, if any
    * - `producers` the producers approved by this voter if no proxy set
    * - `staked` the amount staked
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] voter_info {
      name                owner;     /// the voter
      name                proxy;     /// the proxy set by the voter, if any
      std::vector<name>   producers; /// the producers approved by this voter if no proxy set
      int64_t             staked = 0;
      int64_t             unpaid_votes = 0;
      time_point          last_claim_time;
      /**
       *  Every time a vote is cast we must first "undo" the last vote weight, before casting the
       *  new vote weight.  Vote weight is calculated as:
       *
       *  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
       */
      double              last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

      /**
       * Total vote weight delegated to this voter.
       */
      double              proxied_vote_weight= 0; /// the total vote weight delegated to this voter as a proxy
      bool                is_proxy = 0; /// whether the voter is a proxy for others


      uint32_t            flags1 = 0;
      uint32_t            reserved2 = 0;
      alaio::asset        reserved3;

      uint64_t primary_key()const { return owner.value; }

      enum class flags1_fields : uint32_t {
         ram_managed = 1,
         net_managed = 2,
         cpu_managed = 4
      };

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( voter_info, (owner)(proxy)
                                    (producers)(staked)(unpaid_votes)(last_claim_time)(last_vote_weight)
                                    (proxied_vote_weight)(is_proxy)(flags1)(reserved2)(reserved3) )
   };

   /**
    * Voters table
    *
    * @details The voters table stores all the `voter_info`s instances, all voters information.
    */
   typedef alaio::multi_index< "voters"_n, voter_info >  voters_table;
   
   //campaigns

   static inline uint128_t getVoterCampaignId(uint64_t voter_value, uint64_t campaign_id) {
      return (uint128_t{voter_value} << 64) | campaign_id;
   }

   struct [[alaio::table, alaio::contract("alaio.system")]] campaign_vote_row {            
      uint64_t       id;
      name           voter;
      uint64_t       campaign_id;
      int            vote;
      time_point_sec created_at;
      
      uint64_t primary_key() const { return id; };
      uint128_t by_voter_campaign() const { return getVoterCampaignId(voter.value, campaign_id); }
   };

   typedef alaio::multi_index<"cvotes"_n, campaign_vote_row,
      indexed_by<"votercamp"_n, const_mem_fun<campaign_vote_row, uint128_t, &campaign_vote_row::by_voter_campaign>>
   > campaigns_votes_table;

   struct [[alaio::table, alaio::contract("alaio.system")]] campaign_voter_row {            
      name        voter;
      int64_t     unpaid_campaigns_votes = 0;
      time_point  last_claim_time;
      
      uint64_t primary_key() const { return voter.value; };      
   };

   typedef alaio::multi_index<"campvoters"_n, campaign_voter_row>  campaigns_voters_table;

   //given any timestamp, get same date at 00:00
   static inline uint32_t getDateStartSeconds(uint32_t timeSeconds) {
      //86400 is one day in seconds
      return timeSeconds - (timeSeconds % 86400);      
   }

   struct [[alaio::table, alaio::contract("alaio.system")]] consecutive_voter_row {
      name                    voter;
      std::vector<uint32_t>   datetimes;
      
      uint64_t primary_key() const { return voter.value; };      
   };

   typedef alaio::multi_index<"consecvoters"_n, consecutive_voter_row>  consecutive_voters_table;

   //end campaigns

   struct [[alaio::table, alaio::contract("alaio.system")]] premium_user_row {
      name        user;
      time_point  date_added;
      
      uint64_t primary_key() const { return user.value; };
   };

   typedef alaio::multi_index<"premiumusers"_n, premium_user_row>  premium_users_table;

   struct [[alaio::table, alaio::contract("alaio.system")]] referral_row {
      name        referred;
      name        referrer;
      time_point  date_added;
      time_point  date_rewarded;
      
      uint64_t primary_key() const { return referred.value; };
   };

   typedef alaio::multi_index<"referrals"_n, referral_row>  referrals_table;

   /**
    * Defines producer info table added in version 1.0
    */
   typedef alaio::multi_index< "producers"_n, producer_info,
                               indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                             > producers_table;
   /**
    * Defines new producer info table added in version 1.3.0
    */
   typedef alaio::multi_index< "producers2"_n, producer_info2 > producers_table2;
   typedef alaio::multi_index< "producers3"_n, producer_info3 > producers_table3;

   /**
    * Global state singleton added in version 1.0
    */
   typedef alaio::singleton< "global"_n, alaio_global_state >   global_state_singleton;
   /**
    * Global state singleton added in version 1.1.0
    */
   typedef alaio::singleton< "global2"_n, alaio_global_state2 > global_state2_singleton;
   /**
    * Global state singleton added in version 1.3
    */
   typedef alaio::singleton< "global3"_n, alaio_global_state3 > global_state3_singleton;
   typedef alaio::singleton< "global4"_n, alaio_global_state4 > global_state4_singleton;
   typedef alaio::singleton< "global5"_n, alaio_global_state5 > global_state5_singleton;

   typedef alaio::singleton< "globalbps"_n, alaio_global_state_bps > global_state_bps_singleton;

   struct [[alaio::table, alaio::contract("alaio.system")]] user_resources {
      name          owner;
      asset         net_weight;
      asset         cpu_weight;
      int64_t       ram_bytes = 0;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }
      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes) )
   };

   /**
    *  Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
    */
   struct [[alaio::table, alaio::contract("alaio.system")]] delegated_bandwidth {
      name          from;
      name          to;
      asset         net_weight;
      asset         cpu_weight;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0; }
      uint64_t  primary_key()const { return to.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

   };

   struct [[alaio::table, alaio::contract("alaio.system")]] refund_request {
      name            owner;
      time_point_sec  request_time;
      alaio::asset    net_amount;
      alaio::asset    cpu_amount;

      bool is_empty()const { return net_amount.amount == 0 && cpu_amount.amount == 0; }
      uint64_t  primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      ALALIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
   };

   /**
    *  These tables are designed to be constructed in the scope of the relevant user, this
    *  facilitates simpler API for per-user queries
    */
   typedef alaio::multi_index< "userres"_n, user_resources >      user_resources_table;
   typedef alaio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;
   typedef alaio::multi_index< "refunds"_n, refund_request >      refunds_table;

   /**
    * `rex_pool` structure underlying the rex pool table.
    *
    * @details A rex pool table entry is defined by:
    * - `version` defaulted to zero,
    * - `total_lent` total amount of CORE_SYMBOL in open rex_loans
    * - `total_unlent` total amount of CORE_SYMBOL available to be lent (connector),
    * - `total_rent` fees received in exchange for lent  (connector),
    * - `total_lendable` total amount of CORE_SYMBOL that have been lent (total_unlent + total_lent),
    * - `total_rex` total number of REX shares allocated to contributors to total_lendable,
    * - `namebid_proceeds` the amount of CORE_SYMBOL to be transferred from namebids to REX pool,
    * - `loan_num` increments with each new loan.
    */
   struct [[alaio::table,alaio::contract("alaio.system")]] rex_pool {
      uint8_t    version = 0;
      asset      total_lent; /// total amount of CORE_SYMBOL in open rex_loans
      asset      total_unlent; /// total amount of CORE_SYMBOL available to be lent (connector)
      asset      total_rent; /// fees received in exchange for lent  (connector)
      asset      total_lendable; /// total amount of CORE_SYMBOL that have been lent (total_unlent + total_lent)
      asset      total_rex; /// total number of REX shares allocated to contributors to total_lendable
      asset      namebid_proceeds; /// the amount of CORE_SYMBOL to be transferred from namebids to REX pool
      uint64_t   loan_num = 0; /// increments with each new loan

      uint64_t primary_key()const { return 0; }
   };

   /**
    * Rex pool table
    *
    * @details The rex pool table is storing the only one instance of rex_pool which it stores
    * the global state of the REX system.
    */
   typedef alaio::multi_index< "rexpool"_n, rex_pool > rex_pool_table;

   /**
    * `rex_fund` structure underlying the rex fund table.
    *
    * @details A rex fund table entry is defined by:
    * - `version` defaulted to zero,
    * - `owner` the owner of the rex fund,
    * - `balance` the balance of the fund.
    */
   struct [[alaio::table,alaio::contract("alaio.system")]] rex_fund {
      uint8_t version = 0;
      name    owner;
      asset   balance;

      uint64_t primary_key()const { return owner.value; }
   };

   /**
    * Rex fund table
    *
    * @details The rex fund table is storing all the `rex_fund`s instances.
    */
   typedef alaio::multi_index< "rexfund"_n, rex_fund > rex_fund_table;

   /**
    * `rex_balance` structure underlying the rex balance table.
    *
    * @details A rex balance table entry is defined by:
    * - `version` defaulted to zero,
    * - `owner` the owner of the rex fund,
    * - `vote_stake` the amount of CORE_SYMBOL currently included in owner's vote,
    * - `rex_balance` the amount of REX owned by owner,
    * - `matured_rex` matured REX available for selling.
    */
   struct [[alaio::table,alaio::contract("alaio.system")]] rex_balance {
      uint8_t version = 0;
      name    owner;
      asset   vote_stake; /// the amount of CORE_SYMBOL currently included in owner's vote
      asset   rex_balance; /// the amount of REX owned by owner
      int64_t matured_rex = 0; /// matured REX available for selling
      std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

      uint64_t primary_key()const { return owner.value; }
   };

   /**
    * Rex balance table
    *
    * @details The rex balance table is storing all the `rex_balance`s instances.
    */
   typedef alaio::multi_index< "rexbal"_n, rex_balance > rex_balance_table;

   /**
    * `rex_loan` structure underlying the `rex_cpu_loan_table` and `rex_net_loan_table`.
    *
    * @details A rex net/cpu loan table entry is defined by:
    * - `version` defaulted to zero,
    * - `from` account creating and paying for loan,
    * - `receiver` account receiving rented resources,
    * - `payment` SYS tokens paid for the loan,
    * - `balance` is the amount of SYS tokens available to be used for loan auto-renewal,
    * - `total_staked` total amount staked,
    * - `loan_num` loan number/id,
    * - `expiration` the expiration time when loan will be either closed or renewed
    *       If payment <= balance, the loan is renewed, and closed otherwise.
    */
   struct [[alaio::table,alaio::contract("alaio.system")]] rex_loan {
      uint8_t             version = 0;
      name                from;
      name                receiver;
      asset               payment;
      asset               balance;
      asset               total_staked;
      uint64_t            loan_num;
      alaio::time_point   expiration;

      uint64_t primary_key()const { return loan_num;                   }
      uint64_t by_expr()const     { return expiration.elapsed.count(); }
      uint64_t by_owner()const    { return from.value;                 }
   };

   /**
    * Rex cpu loan table
    *
    * @details The rex cpu loan table is storing all the `rex_loan`s instances for cpu, indexed by loan number, expiration and owner.
    */
   typedef alaio::multi_index< "cpuloan"_n, rex_loan,
                               indexed_by<"byexpr"_n,  const_mem_fun<rex_loan, uint64_t, &rex_loan::by_expr>>,
                               indexed_by<"byowner"_n, const_mem_fun<rex_loan, uint64_t, &rex_loan::by_owner>>
                             > rex_cpu_loan_table;

   /**
    * Rex net loan table
    *
    * @details The rex net loan table is storing all the `rex_loan`s instances for net, indexed by loan number, expiration and owner.
    */
   typedef alaio::multi_index< "netloan"_n, rex_loan,
                               indexed_by<"byexpr"_n,  const_mem_fun<rex_loan, uint64_t, &rex_loan::by_expr>>,
                               indexed_by<"byowner"_n, const_mem_fun<rex_loan, uint64_t, &rex_loan::by_owner>>
                             > rex_net_loan_table;

   struct [[alaio::table,alaio::contract("alaio.system")]] rex_order {
      uint8_t             version = 0;
      name                owner;
      asset               rex_requested;
      asset               proceeds;
      asset               stake_change;
      alaio::time_point   order_time;
      bool                is_open = true;

      void close()                { is_open = false;    }
      uint64_t primary_key()const { return owner.value; }
      uint64_t by_time()const     { return is_open ? order_time.elapsed.count() : std::numeric_limits<uint64_t>::max(); }
   };

   /**
    * Rex order table
    *
    * @details The rex order table is storing all the `rex_order`s instances, indexed by owner and time and owner.
    */
   typedef alaio::multi_index< "rexqueue"_n, rex_order,
                               indexed_by<"bytime"_n, const_mem_fun<rex_order, uint64_t, &rex_order::by_time>>> rex_order_table;

   struct rex_order_outcome {
      bool success;
      asset proceeds;
      asset stake_change;
   };

enum class response_type : uint16_t { Bool = 0, Int, Double, String, MaxVal };
   enum class request_type : uint16_t { GET = 0, POST };
   enum class aggregation : uint16_t { MEAN = 0, STD, BOOLEAN };


   struct [[alaio::table, alaio::contract("alaio.system")]] oracle_info {
      name       producer;
      name       oracle_account;
      uint32_t   pending_requests;
      uint32_t   successful_requests;
      uint32_t   failed_requests;
      int64_t    pending_punishment;

      uint64_t primary_key() const { return producer.value; }
      uint64_t by_oracle_account() const { return oracle_account.value; }

      bool is_active() const {
         return oracle_account.value != 0;
      }
   };
   typedef alaio::multi_index< "oracles"_n, oracle_info,
      indexed_by<"oracleacc"_n, const_mem_fun<oracle_info, uint64_t, &oracle_info::by_oracle_account>  >
   > oracle_info_table;


   struct api {
      std::string endpoint;
      // uint16_t request_type;
      // uint16_t response_type;
      // std::string parameters;
      std::string json_field;
   };

   struct [[alaio::table, alaio::contract("alaio.system")]] request_info {
      uint64_t         id;
      time_point       time;
      name             assigned_oracle;
      name             standby_oracle;
      std::vector<api> apis;
      uint16_t         response_type;
      uint16_t         aggregation_type;
      uint16_t         prefered_api;
      std::string      string_to_count;

      uint64_t primary_key() const { return id; }
   };
   typedef alaio::multi_index< "requests"_n, request_info > request_info_table;


   struct [[alaio::table("oraclereward"), alaio::contract("alaio.system")]] oracle_reward_info {
      uint32_t total_successful_requests = 0;
   };
   typedef alaio::singleton< "oraclereward"_n, oracle_reward_info > oracle_reward_info_singleton;

   /**
    * The ALAIO system contract.
    *
    * @details The ALAIO system contract governs ram market, voters, producers, global state.
    */
   class [[alaio::contract("alaio.system")]] system_contract : public native {

      private:
         voters_table            _voters;
         producers_table         _producers;
         producers_table2        _producers2;
         producers_table3        _producers3;
         
         campaigns_votes_table      _campaigns_votes;
         campaigns_voters_table     _campaigns_voters;
         consecutive_voters_table   _consecutive_voters;

         premium_users_table     _premium_users;
         referrals_table         _referrals;
         
         global_state_singleton  _global;
         global_state2_singleton _global2;
         global_state3_singleton _global3;
         global_state4_singleton _global4;
         global_state5_singleton _global5;

         global_state_bps_singleton _globalbps;

         alaio_global_state      _gstate;
         alaio_global_state2     _gstate2;
         alaio_global_state3     _gstate3;
         alaio_global_state4     _gstate4;
         alaio_global_state5     _gstate5;

         alaio_global_state_bps  _gstatebps;

         rammarket               _rammarket;
         rex_pool_table          _rexpool;
         rex_fund_table          _rexfunds;
         rex_balance_table       _rexbalance;
         rex_order_table         _rexorders;
         oracle_reward_info_singleton _oracle_global;
         oracle_reward_info           _oracle_state;
         oracle_info_table            _oracles;

      public:
         static constexpr alaio::name active_permission{"active"_n};
         static constexpr alaio::name token_account{"alaio.token"_n};
         static constexpr alaio::name ram_account{"alaio.ram"_n};
         static constexpr alaio::name ramfee_account{"alaio.ramfee"_n};
         static constexpr alaio::name stake_account{"alaio.stake"_n};
         static constexpr alaio::name bpay_account{"alaio.bpay"_n};     // stores tokens for Block producers rewards
         // static constexpr alaio::name communitybpay_account{"alaio.cpay"_n};     // stores tokens for Block Producers's 44% rewards 
         // static constexpr alaio::name dpay_account{"alaio.dpay"_n};     // stores tokens for DApps developers rewards
         static constexpr alaio::name dpay_transfer_account{"alaio.dtpay"_n};     // stores tokens for DApps developers rewards
         static constexpr alaio::name dpay_user_account{"alaio.dupay"_n};     // stores tokens for DApps developers rewards
         static constexpr alaio::name wpay_account{"alaio.wpay"_n};     // stores tokens for Witnesses rewards
         static constexpr alaio::name vpay_account{"alaio.vpay"_n};     // stores tokens for Voters rewards
         static constexpr alaio::name oracle_account{"alaio.opay"_n}; // stores tokens for Oracle rewards
         static constexpr alaio::name network_account{"alaio.net"_n};
         static constexpr alaio::name admin_account{"alaio.admin"_n};   // Admin account that distribute the tokens to all the users
         static constexpr alaio::name infrastructure_account{"alaio.infra"_n};
         static constexpr alaio::name community_account{"alaio.comm"_n};
         static constexpr alaio::name marketing_account{"alaio.market"_n};
         static constexpr alaio::name founding_account{"alaio.found"_n};
         static constexpr alaio::name names_account{"alaio.names"_n};
         // static constexpr alaio::name saving_account{"alaio.saving"_n};
         static constexpr alaio::name rex_account{"alaio.rex"_n};
         static constexpr alaio::name null_account{"alaio.null"_n};
         static constexpr symbol ramcore_symbol = symbol(symbol_code("RAMCORE"), 4);
         static constexpr symbol ram_symbol     = symbol(symbol_code("RAM"), 0);
         static constexpr symbol rex_symbol     = symbol(symbol_code("REX"), 4);
         static constexpr alaio::name system_account{"alaio"_n};

         /**
          * System contract constructor.
          *
          * @details Constructs a system contract based on self account, code account and data.
          *
          * @param s    - The current code account that is executing the action,
          * @param code - The original code account that executed the action,
          * @param ds   - The contract data represented as an `alaio::datastream`.
          */
         system_contract( name s, name code, datastream<const char*> ds );
         ~system_contract();

         /**
          * Returns the core symbol by system account name
          *
          * @param system_account - the system account to get the core symbol for.
          */
         static symbol get_core_symbol( name system_account = "alaio"_n ) {
            rammarket rm(system_account, system_account.value);
            const static auto sym = get_core_symbol( rm );
            return sym;
         }

         static std::vector<alaio::name> get_top_producers() {
            producers_table prodtable(system_account, system_account.value);
            auto idx = prodtable.get_index<"prototalvote"_n>();            
            std::vector<alaio::name> top_producers;
            top_producers.reserve(21);
            for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && it->active(); ++it ) {
               top_producers.emplace_back( it->owner );
            }
            /// sort by producer name
            std::sort( top_producers.begin(), top_producers.end() );
            return top_producers;
         }

         // Actions:
         /**
          * Init action.
          *
          * @details Init action initializes the system contract for a version and a symbol.
          * Only succeeds when:
          * - version is 0 and
          * - symbol is found and
          * - system token supply is greater than 0,
          * - and system contract wasn’t already been initialized.
          *
          * @param version - the version, has to be 0,
          * @param core - the system symbol.
          */
         [[alaio::action]]
         void init( unsigned_int version, const symbol& core );

         /**
          * On block action.
          *
          * @details This special action is triggered when a block is applied by the given producer
          * and cannot be generated from any other source. It is used to pay producers and calculate
          * missed blocks of other producers. Producer pay is deposited into the producer's stake
          * balance and can be withdrawn over time. If blocknum is the start of a new round this may
          * update the active producer config from the producer votes.
          *
          * @param header - the block header produced.
          */
         [[alaio::action]]
         void onblock( ignore<block_header> header );

         /**
          * Set account limits action.
          *
          * @details Set the resource limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param ram_bytes - ram limit in absolute bytes,
          * @param net_weight - fractionally proportionate net limit of available resources based on (weight / total_weight_of_all_accounts),
          * @param cpu_weight - fractionally proportionate cpu limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[alaio::action]]
         void setalimits( const name& account, int64_t ram_bytes, int64_t net_weight, int64_t cpu_weight );

         /**
          * Set account RAM limits action.
          *
          * @details Set the RAM limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param ram_bytes - ram limit in absolute bytes.
          */
         [[alaio::action]]
         void setacctram( const name& account, const std::optional<int64_t>& ram_bytes );

         /**
          * Set account NET limits action.
          *
          * @details Set the NET limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param net_weight - fractionally proportionate net limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[alaio::action]]
         void setacctnet( const name& account, const std::optional<int64_t>& net_weight );

         /**
          * Set account CPU limits action.
          *
          * @details Set the CPU limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param cpu_weight - fractionally proportionate cpu limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[alaio::action]]
         void setacctcpu( const name& account, const std::optional<int64_t>& cpu_weight );


         /**
          * Activates a protocol feature.
          *
          * @details Activates a protocol feature
          *
          * @param feature_digest - hash of the protocol feature to activate.
          */
         [[alaio::action]]
         void activate( const alaio::checksum256& feature_digest );

         // functions defined in oracle.cpp

         [[alaio::action]]
         void addrequest( uint64_t request_id, const alaio::name& caller, const std::vector<api>& apis, uint16_t response_type, uint16_t aggregation_type, uint16_t prefered_api, std::string string_to_count );

         [[alaio::action]]
         void reply( const alaio::name& caller, uint64_t request_id, const std::vector<char>& response );

         [[alaio::action]]
         void setoracle( const alaio::name& producer);

         // functions defined in delegate_bandwidth.cpp

         /**
          * Delegate bandwidth and/or cpu action.
          *
          * @details Stakes SYS from the balance of `from` for the benefit of `receiver`.
          *
          * @param from - the account to delegate bandwidth from, that is, the account holding
          *    tokens to be staked,
          * @param receiver - the account to delegate bandwith to, that is, the account to
          *    whose resources staked tokens are added
          * @param stake_net_quantity - tokens staked for NET bandwidth,
          * @param stake_cpu_quantity - tokens staked for CPU bandwidth,
          * @param transfer - if true, ownership of staked tokens is transfered to `receiver`.
          *
          * @post All producers `from` account has voted for will have their votes updated immediately.
          */
         [[alaio::action]]
         void delegatebw( const name& from, const name& receiver,
                          const asset& stake_net_quantity, const asset& stake_cpu_quantity, bool transfer );

         /**
          * Setrex action.
          *
          * @details Sets total_rent balance of REX pool to the passed value.
          * @param balance - amount to set the REX pool balance.
          */
         [[alaio::action]]
         void setrex( const asset& balance );

         /**
          * Deposit to REX fund action.
          *
          * @details Deposits core tokens to user REX fund.
          * All proceeds and expenses related to REX are added to or taken out of this fund.
          * An inline transfer from 'owner' liquid balance is executed.
          * All REX-related costs and proceeds are deducted from and added to 'owner' REX fund,
          *    with one exception being buying REX using staked tokens.
          * Storage change is billed to 'owner'.
          *
          * @param owner - REX fund owner account,
          * @param amount - amount of tokens to be deposited.
          */
         [[alaio::action]]
         void deposit( const name& owner, const asset& amount );

         /**
          * Withdraw from REX fund action.
          *
          * @details Withdraws core tokens from user REX fund.
          * An inline token transfer to user balance is executed.
          *
          * @param owner - REX fund owner account,
          * @param amount - amount of tokens to be withdrawn.
          */
         [[alaio::action]]
         void withdraw( const name& owner, const asset& amount );

         /**
          * Buyrex action.
          *
          * @details Buys REX in exchange for tokens taken out of user's REX fund by transfering
          * core tokens from user REX fund and converts them to REX stake. By buying REX, user is
          * lending tokens in order to be rented as CPU or NET resourses.
          * Storage change is billed to 'from' account.
          *
          * @param from - owner account name,
          * @param amount - amount of tokens taken out of 'from' REX fund.
          *
          * @pre A voting requirement must be satisfied before action can be executed.
          * @pre User must vote for at least 21 producers or delegate vote to proxy before buying REX.
          *
          * @post User votes are updated following this action.
          * @post Tokens used in purchase are added to user's voting power.
          * @post Bought REX cannot be sold before 4 days counting from end of day of purchase.
          */
         [[alaio::action]]
         void buyrex( const name& from, const asset& amount );

         /**
          * Unstaketorex action.
          *
          * @details Use staked core tokens to buy REX.
          * Storage change is billed to 'owner' account.
          *
          * @param owner - owner of staked tokens,
          * @param receiver - account name that tokens have previously been staked to,
          * @param from_net - amount of tokens to be unstaked from NET bandwidth and used for REX purchase,
          * @param from_cpu - amount of tokens to be unstaked from CPU bandwidth and used for REX purchase.
          *
          * @pre A voting requirement must be satisfied before action can be executed.
          * @pre User must vote for at least 21 producers or delegate vote to proxy before buying REX.
          *
          * @post User votes are updated following this action.
          * @post Tokens used in purchase are added to user's voting power.
          * @post Bought REX cannot be sold before 4 days counting from end of day of purchase.
          */
         [[alaio::action]]
         void unstaketorex( const name& owner, const name& receiver, const asset& from_net, const asset& from_cpu );

         /**
          * Sellrex action.
          *
          * @details Sells REX in exchange for core tokens by converting REX stake back into core tokens
          * at current exchange rate. If order cannot be processed, it gets queued until there is enough
          * in REX pool to fill order, and will be processed within 30 days at most. If successful, user
          * votes are updated, that is, proceeds are deducted from user's voting power. In case sell order
          * is queued, storage change is billed to 'from' account.
          *
          * @param from - owner account of REX,
          * @param rex - amount of REX to be sold.
          */
         [[alaio::action]]
         void sellrex( const name& from, const asset& rex );

         /**
          * Cnclrexorder action.
          *
          * @details Cancels unfilled REX sell order by owner if one exists.
          *
          * @param owner - owner account name.
          *
          * @pre Order cannot be cancelled once it's been filled.
          */
         [[alaio::action]]
         void cnclrexorder( const name& owner );

         /**
          * Rentcpu action.
          *
          * @details Use payment to rent as many SYS tokens as possible as determined by market price and
          * stake them for CPU for the benefit of receiver, after 30 days the rented core delegation of CPU
          * will expire. At expiration, if balance is greater than or equal to `loan_payment`, `loan_payment`
          * is taken out of loan balance and used to renew the loan. Otherwise, the loan is closed and user
          * is refunded any remaining balance.
          * Owner can fund or refund a loan at any time before its expiration.
          * All loan expenses and refunds come out of or are added to owner's REX fund.
          *
          * @param from - account creating and paying for CPU loan, 'from' account can add tokens to loan
          *    balance using action `fundcpuloan` and withdraw from loan balance using `defcpuloan`
          * @param receiver - account receiving rented CPU resources,
          * @param loan_payment - tokens paid for the loan, it has to be greater than zero,
          *    amount of rented resources is calculated from `loan_payment`,
          * @param loan_fund - additional tokens can be zero, and is added to loan balance.
          *    Loan balance represents a reserve that is used at expiration for automatic loan renewal.
          */
         [[alaio::action]]
         void rentcpu( const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );

         /**
          * Rentnet action.
          *
          * @details Use payment to rent as many SYS tokens as possible as determined by market price and
          * stake them for NET for the benefit of receiver, after 30 days the rented core delegation of NET
          * will expire. At expiration, if balance is greater than or equal to `loan_payment`, `loan_payment`
          * is taken out of loan balance and used to renew the loan. Otherwise, the loan is closed and user
          * is refunded any remaining balance.
          * Owner can fund or refund a loan at any time before its expiration.
          * All loan expenses and refunds come out of or are added to owner's REX fund.
          *
          * @param from - account creating and paying for NET loan, 'from' account can add tokens to loan
          *    balance using action `fundnetloan` and withdraw from loan balance using `defnetloan`,
          * @param receiver - account receiving rented NET resources,
          * @param loan_payment - tokens paid for the loan, it has to be greater than zero,
          *    amount of rented resources is calculated from `loan_payment`,
          * @param loan_fund - additional tokens can be zero, and is added to loan balance.
          *    Loan balance represents a reserve that is used at expiration for automatic loan renewal.
          */
         [[alaio::action]]
         void rentnet( const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );

         /**
          * Fundcpuloan action.
          *
          * @details Transfers tokens from REX fund to the fund of a specific CPU loan in order to
          * be used for loan renewal at expiry.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param payment - tokens transfered from REX fund to loan fund.
          */
         [[alaio::action]]
         void fundcpuloan( const name& from, uint64_t loan_num, const asset& payment );

         /**
          * Fundnetloan action.
          *
          * @details Transfers tokens from REX fund to the fund of a specific NET loan in order to
          * be used for loan renewal at expiry.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param payment - tokens transfered from REX fund to loan fund.
          */
         [[alaio::action]]
         void fundnetloan( const name& from, uint64_t loan_num, const asset& payment );

         /**
          * Defcpuloan action.
          *
          * @details Withdraws tokens from the fund of a specific CPU loan and adds them to REX fund.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param amount - tokens transfered from CPU loan fund to REX fund.
          */
         [[alaio::action]]
         void defcpuloan( const name& from, uint64_t loan_num, const asset& amount );

         /**
          * Defnetloan action.
          *
          * @details Withdraws tokens from the fund of a specific NET loan and adds them to REX fund.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param amount - tokens transfered from NET loan fund to REX fund.
          */
         [[alaio::action]]
         void defnetloan( const name& from, uint64_t loan_num, const asset& amount );

         /**
          * Updaterex action.
          *
          * @details Updates REX owner vote weight to current value of held REX tokens.
          *
          * @param owner - REX owner account.
          */
         [[alaio::action]]
         void updaterex( const name& owner );

         /**
          * Rexexec action.
          *
          * @details Processes max CPU loans, max NET loans, and max queued sellrex orders.
          * Action does not execute anything related to a specific user.
          *
          * @param user - any account can execute this action,
          * @param max - number of each of CPU loans, NET loans, and sell orders to be processed.
          */
         [[alaio::action]]
         void rexexec( const name& user, uint16_t max );

         /**
          * Consolidate action.
          *
          * @details Consolidates REX maturity buckets into one bucket that can be sold after 4 days
          * starting from the end of the day.
          *
          * @param owner - REX owner account name.
          */
         [[alaio::action]]
         void consolidate( const name& owner );

         /**
          * Mvtosavings action.
          *
          * @details Moves a specified amount of REX into savings bucket. REX savings bucket
          * never matures. In order for it to be sold, it has to be moved explicitly
          * out of that bucket. Then the moved amount will have the regular maturity
          * period of 4 days starting from the end of the day.
          *
          * @param owner - REX owner account name.
          * @param rex - amount of REX to be moved.
          */
         [[alaio::action]]
         void mvtosavings( const name& owner, const asset& rex );

         /**
          * Mvfrsavings action.
          *
          * @details Moves a specified amount of REX out of savings bucket. The moved amount
          * will have the regular REX maturity period of 4 days.
          *
          * @param owner - REX owner account name.
          * @param rex - amount of REX to be moved.
          */
         [[alaio::action]]
         void mvfrsavings( const name& owner, const asset& rex );

         /**
          * Closerex action.
          *
          * @details Deletes owner records from REX tables and frees used RAM. Owner must not have
          * an outstanding REX balance.
          *
          * @param owner - user account name.
          *
          * @pre If owner has a non-zero REX balance, the action fails; otherwise,
          *    owner REX balance entry is deleted.
          * @pre If owner has no outstanding loans and a zero REX fund balance,
          *    REX fund entry is deleted.
          */
         [[alaio::action]]
         void closerex( const name& owner );

         /**
          * Undelegate bandwitdh action.
          *
          * @details Decreases the total tokens delegated by `from` to `receiver` and/or
          * frees the memory associated with the delegation if there is nothing
          * left to delegate.
          * This will cause an immediate reduction in net/cpu bandwidth of the
          * receiver.
          * A transaction is scheduled to send the tokens back to `from` after
          * the staking period has passed. If existing transaction is scheduled, it
          * will be canceled and a new transaction issued that has the combined
          * undelegated amount.
          * The `from` account loses voting power as a result of this call and
          * all producer tallies are updated.
          *
          * @param from - the account to undelegate bandwidth from, that is,
          *    the account whose tokens will be unstaked,
          * @param receiver - the account to undelegate bandwith to, that is,
          *    the account to whose benefit tokens have been staked,
          * @param unstake_net_quantity - tokens to be unstaked from NET bandwidth,
          * @param unstake_cpu_quantity - tokens to be unstaked from CPU bandwidth,
          *
          * @post Unstaked tokens are transferred to `from` liquid balance via a
          *    deferred transaction with a delay of 3 days.
          * @post If called during the delay period of a previous `undelegatebw`
          *    action, pending action is canceled and timer is reset.
          * @post All producers `from` account has voted for will have their votes updated immediately.
          * @post Bandwidth and storage for the deferred transaction are billed to `from`.
          */
         [[alaio::action]]
         void undelegatebw( const name& from, const name& receiver,
                            const asset& unstake_net_quantity, const asset& unstake_cpu_quantity );

         /**
          * Buy ram action.
          *
          * @details Increases receiver's ram quota based upon current price and quantity of
          * tokens provided. An inline transfer from receiver to system contract of
          * tokens will be executed.
          *
          * @param payer - the ram buyer,
          * @param receiver - the ram receiver,
          * @param quant - the quntity of tokens to buy ram with.
          */
         [[alaio::action]]
         void buyram( const name& payer, const name& receiver, const asset& quant );

         /**
          * Buy a specific amount of ram bytes action.
          *
          * @details Increases receiver's ram in quantity of bytes provided.
          * An inline transfer from receiver to system contract of tokens will be executed.
          *
          * @param payer - the ram buyer,
          * @param receiver - the ram receiver,
          * @param bytes - the quntity of ram to buy specified in bytes.
          */
         [[alaio::action]]
         void buyrambytes( const name& payer, const name& receiver, uint32_t bytes );

         /**
          * Sell ram action.
          *
          * @details Reduces quota by bytes and then performs an inline transfer of tokens
          * to receiver based upon the average purchase price of the original quota.
          *
          * @param account - the ram seller account,
          * @param bytes - the amount of ram to sell in bytes.
          */
         [[alaio::action]]
         void sellram( const name& account, int64_t bytes );

         /**
          * Refund action.
          *
          * @details This action is called after the delegation-period to claim all pending
          * unstaked tokens belonging to owner.
          *
          * @param owner - the owner of the tokens claimed.
          */
         [[alaio::action]]
         void refund( const name& owner );

         // functions defined in campaign.cpp

         [[alaio::action]]
         void votecampaign( const name voter, uint64_t campaign_id, int vote );     

         [[alaio::action]]
         void resetcamps();

         [[alaio::action]]
         void testconsec(const name voter);

         [[alaio::action]]
         void resetcons();

         [[alaio::action]]
         void resetausers();

         [[alaio::action]]
         void resetunpaid();

         // functions defined in premium_users.cpp

         [[alaio::action]]
         void addpuser(const name user);

         [[alaio::action]]
         void removepuser(const name user);

         [[alaio::action]]
         void addreferral(const name referred, const name referrer);

         [[alaio::action]]
         void rewreferrer(const name referred, asset quantity);

         // functions defined in voting.cpp

         /**
          * Register producer action.
          *
          * @details Register producer action, indicates that a particular account wishes to become a producer,
          * this action will create a `producer_config` and a `producer_info` object for `producer` scope
          * in producers tables.
          *
          * @param producer - account registering to be a producer candidate,
          * @param producer_key - the public key of the block producer, this is the key used by block producer to sign blocks,
          * @param url - the url of the block producer, normally the url of the block producer presentation website,
          * @param location - is the country code as defined in the ISO 3166, https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
          *
          * @pre Producer is not already registered
          * @pre Producer to register is an account
          * @pre Authority of producer to register
          */
         [[alaio::action]]
         void regproducer( const name& producer, const public_key& producer_key, const std::string& url, uint16_t location );

         /**
          * Unregister producer action.
          *
          * @details Deactivate the block producer with account name `producer`.
          * @param producer - the block producer account to unregister.
          */
         [[alaio::action]]
         void unregprod( const name& producer );

         /**
          * Set ram action.
          *
          * @details Set the ram supply.
          * @param max_ram_size - the amount of ram supply to set.
          */
         [[alaio::action]]
         void setram( uint64_t max_ram_size );

         /**
          * Set ram rate action.

          * @details Sets the rate of increase of RAM in bytes per block. It is capped by the uint16_t to
          * a maximum rate of 3 TB per year. If update_ram_supply hasn't been called for the most recent block,
          * then new ram will be allocated at the old rate up to the present block before switching the rate.
          *
          * @param bytes_per_block - the amount of bytes per block increase to set.
          */
         [[alaio::action]]
         void setramrate( uint16_t bytes_per_block );

         /**
          * Vote producer action.
          *
          * @details Votes for a set of producers. This action updates the list of `producers` voted for,
          * for `voter` account. If voting for a `proxy`, the producer votes will not change until the
          * proxy updates their own vote. Voter can vote for a proxy __or__ a list of at most 30 producers.
          * Storage change is billed to `voter`.
          *
          * @param voter - the account to change the voted producers for,
          * @param proxy - the proxy to change the voted producers for,
          * @param producers - the list of producers to vote for, a maximum of 30 producers is allowed.
          *
          * @pre Producers must be sorted from lowest to highest and must be registered and active
          * @pre If proxy is set then no producers can be voted for
          * @pre If proxy is set then proxy account must exist and be registered as a proxy
          * @pre Every listed producer or proxy must have been previously registered
          * @pre Voter must authorize this action
          * @pre Voter must have previously staked some ALA for voting
          * @pre Voter->staked must be up to date
          *
          * @post Every producer previously voted for will have vote reduced by previous vote weight
          * @post Every producer newly voted for will have vote increased by new vote amount
          * @post Prior proxy will proxied_vote_weight decremented by previous vote weight
          * @post New proxy will proxied_vote_weight incremented by new vote weight
          */
         [[alaio::action]]
         void voteproducer( const name& voter, const name& proxy, const std::vector<name>& producers );

         /**
          * Register proxy action.
          *
          * @details Set `proxy` account as proxy.
          * An account marked as a proxy can vote with the weight of other accounts which
          * have selected it as a proxy. Other accounts must refresh their voteproducer to
          * update the proxy's weight.
          * Storage change is billed to `proxy`.
          *
          * @param rpoxy - the account registering as voter proxy (or unregistering),
          * @param isproxy - if true, proxy is registered; if false, proxy is unregistered.
          *
          * @pre Proxy must have something staked (existing row in voters table)
          * @pre New state must be different than current state
          */
         [[alaio::action]]
         void regproxy( const name& proxy, bool isproxy );

         /**
          * Set the blockchain parameters
          *
          * @details Set the blockchain parameters. By tunning these parameters a degree of
          * customization can be achieved.
          * @param params - New blockchain parameters to set.
          */
         [[alaio::action]]
         void setparams( const alaio::blockchain_parameters& params );

         // functions defined in producer_pay.cpp
         /**
          * Claim rewards action.
          *
          * @details Claim block producing and vote rewards.
          * @param owner - producer account claiming per-block and per-vote rewards.
          */
         [[alaio::action]]
         void claimrewards( const name& owner );

         [[alaio::action]]
         void claimdapprwd( const name dapp_registry, const name owner, const name dapp );

         [[alaio::action]]
         void claimvoterwd(const name owner);

         [[alaio::action]]
         void setvclaimprd( uint32_t period_in_days );

         /**
          * Set privilege status for an account.
          *
          * @details Allows to set privilege status for an account (turn it on/off).
          * @param account - the account to set the privileged status for.
          * @param is_priv - 0 for false, > 0 for true.
          */
         [[alaio::action]]
         void setpriv( const name& account, uint8_t is_priv );

         /**
          * Remove producer action.
          *
          * @details Deactivates a producer by name, if not found asserts.
          * @param producer - the producer account to deactivate.
          */
         [[alaio::action]]
         void rmvproducer( const name& producer );

         /**
          * Update revision action.
          *
          * @details Updates the current revision.
          * @param revision - it has to be incremented by 1 compared with current revision.
          *
          * @pre Current revision can not be higher than 254, and has to be smaller
          * than or equal 1 (“set upper bound to greatest revision supported in the code”).
          */
         [[alaio::action]]
         void updtrevision( uint8_t revision );

         /**
          * Bid name action.
          *
          * @details Allows an account `bidder` to place a bid for a name `newname`.
          * @param bidder - the account placing the bid,
          * @param newname - the name the bid is placed for,
          * @param bid - the amount of system tokens payed for the bid.
          *
          * @pre Bids can be placed only on top-level suffix,
          * @pre Non empty name,
          * @pre Names longer than 12 chars are not allowed,
          * @pre Names equal with 12 chars can be created without placing a bid,
          * @pre Bid has to be bigger than zero,
          * @pre Bid's symbol must be system token,
          * @pre Bidder account has to be different than current highest bidder,
          * @pre Bid must increase current bid by 10%,
          * @pre Auction must still be opened.
          */
         [[alaio::action]]
         void bidname( const name& bidder, const name& newname, const asset& bid );

         /**
          * Bid refund action.
          *
          * @details Allows the account `bidder` to get back the amount it bid so far on a `newname` name.
          *
          * @param bidder - the account that gets refunded,
          * @param newname - the name for which the bid was placed and now it gets refunded for.
          */
         [[alaio::action]]
         void bidrefund( const name& bidder, const name& newname );

         using init_action = alaio::action_wrapper<"init"_n, &system_contract::init>;
         using setacctram_action = alaio::action_wrapper<"setacctram"_n, &system_contract::setacctram>;
         using setacctnet_action = alaio::action_wrapper<"setacctnet"_n, &system_contract::setacctnet>;
         using setacctcpu_action = alaio::action_wrapper<"setacctcpu"_n, &system_contract::setacctcpu>;
         using activate_action = alaio::action_wrapper<"activate"_n, &system_contract::activate>;
         using delegatebw_action = alaio::action_wrapper<"delegatebw"_n, &system_contract::delegatebw>;
         using deposit_action = alaio::action_wrapper<"deposit"_n, &system_contract::deposit>;
         using withdraw_action = alaio::action_wrapper<"withdraw"_n, &system_contract::withdraw>;
         using buyrex_action = alaio::action_wrapper<"buyrex"_n, &system_contract::buyrex>;
         using unstaketorex_action = alaio::action_wrapper<"unstaketorex"_n, &system_contract::unstaketorex>;
         using sellrex_action = alaio::action_wrapper<"sellrex"_n, &system_contract::sellrex>;
         using cnclrexorder_action = alaio::action_wrapper<"cnclrexorder"_n, &system_contract::cnclrexorder>;
         using rentcpu_action = alaio::action_wrapper<"rentcpu"_n, &system_contract::rentcpu>;
         using rentnet_action = alaio::action_wrapper<"rentnet"_n, &system_contract::rentnet>;
         using fundcpuloan_action = alaio::action_wrapper<"fundcpuloan"_n, &system_contract::fundcpuloan>;
         using fundnetloan_action = alaio::action_wrapper<"fundnetloan"_n, &system_contract::fundnetloan>;
         using defcpuloan_action = alaio::action_wrapper<"defcpuloan"_n, &system_contract::defcpuloan>;
         using defnetloan_action = alaio::action_wrapper<"defnetloan"_n, &system_contract::defnetloan>;
         using updaterex_action = alaio::action_wrapper<"updaterex"_n, &system_contract::updaterex>;
         using rexexec_action = alaio::action_wrapper<"rexexec"_n, &system_contract::rexexec>;
         using setrex_action = alaio::action_wrapper<"setrex"_n, &system_contract::setrex>;
         using mvtosavings_action = alaio::action_wrapper<"mvtosavings"_n, &system_contract::mvtosavings>;
         using mvfrsavings_action = alaio::action_wrapper<"mvfrsavings"_n, &system_contract::mvfrsavings>;
         using consolidate_action = alaio::action_wrapper<"consolidate"_n, &system_contract::consolidate>;
         using closerex_action = alaio::action_wrapper<"closerex"_n, &system_contract::closerex>;
         using undelegatebw_action = alaio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;
         using buyram_action = alaio::action_wrapper<"buyram"_n, &system_contract::buyram>;
         using buyrambytes_action = alaio::action_wrapper<"buyrambytes"_n, &system_contract::buyrambytes>;
         using sellram_action = alaio::action_wrapper<"sellram"_n, &system_contract::sellram>;
         using refund_action = alaio::action_wrapper<"refund"_n, &system_contract::refund>;
         using regproducer_action = alaio::action_wrapper<"regproducer"_n, &system_contract::regproducer>;
         using unregprod_action = alaio::action_wrapper<"unregprod"_n, &system_contract::unregprod>;
         using setram_action = alaio::action_wrapper<"setram"_n, &system_contract::setram>;
         using setramrate_action = alaio::action_wrapper<"setramrate"_n, &system_contract::setramrate>;
         using voteproducer_action = alaio::action_wrapper<"voteproducer"_n, &system_contract::voteproducer>;
         using votecampaign_action = alaio::action_wrapper<"votecampaign"_n, &system_contract::votecampaign>;
         using resetcamps_action = alaio::action_wrapper<"resetcamps"_n, &system_contract::resetcamps>;
         using testconsec_action = alaio::action_wrapper<"testconsec"_n, &system_contract::testconsec>;         
         using resetcons_action = alaio::action_wrapper<"resetcons"_n, &system_contract::resetcons>;
         using resetausers_action = alaio::action_wrapper<"resetausers"_n, &system_contract::resetausers>;
         using resetunpaid_action = alaio::action_wrapper<"resetunpaid"_n, &system_contract::resetunpaid>;
         using regproxy_action = alaio::action_wrapper<"regproxy"_n, &system_contract::regproxy>;
         using claimrewards_action = alaio::action_wrapper<"claimrewards"_n, &system_contract::claimrewards>;
         using claimdapprwd_action = alaio::action_wrapper<"claimdapprwd"_n, &system_contract::claimdapprwd>;
         using claimvoterwd_action = alaio::action_wrapper<"claimvoterwd"_n, &system_contract::claimvoterwd>;
         using setvclaimprd_action = alaio::action_wrapper<"setvclaimprd"_n, &system_contract::setvclaimprd>;
         using rmvproducer_action = alaio::action_wrapper<"rmvproducer"_n, &system_contract::rmvproducer>;
         using updtrevision_action = alaio::action_wrapper<"updtrevision"_n, &system_contract::updtrevision>;
         using bidname_action = alaio::action_wrapper<"bidname"_n, &system_contract::bidname>;
         using bidrefund_action = alaio::action_wrapper<"bidrefund"_n, &system_contract::bidrefund>;
         using setpriv_action = alaio::action_wrapper<"setpriv"_n, &system_contract::setpriv>;
         using setalimits_action = alaio::action_wrapper<"setalimits"_n, &system_contract::setalimits>;
         using setparams_action = alaio::action_wrapper<"setparams"_n, &system_contract::setparams>;

      private:
         // Implementation details:

         static symbol get_core_symbol( const rammarket& rm ) {
            auto itr = rm.find(ramcore_symbol.raw());
            check(itr != rm.end(), "system contract must first be initialized");
            return itr->quote.balance.symbol;
         }

         //defined in alaio.system.cpp
         static alaio_global_state get_default_parameters();
         
         static time_point_sec current_time_point_sec();
         
         symbol core_symbol()const;
         void update_ram_supply();

         //defined in campaign.cpp
         void saveVoteHistory(const name voter);
         bool shouldGetVoteReward(const name voter);         

         // defined in rex.cpp
         void runrex( uint16_t max );
         void update_resource_limits( const name& from, const name& receiver, int64_t delta_net, int64_t delta_cpu );
         void check_voting_requirement( const name& owner,
                                        const char* error_msg = "must vote for at least 21 producers or for a proxy before buying REX" )const;
         rex_order_outcome fill_rex_order( const rex_balance_table::const_iterator& bitr, const asset& rex );
         asset update_rex_account( const name& owner, const asset& proceeds, const asset& unstake_quant, bool force_vote_update = false );
         void channel_to_rex( const name& from, const asset& amount );
         void channel_namebid_to_rex( const int64_t highest_bid );
         template <typename T>
         int64_t rent_rex( T& table, const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );
         template <typename T>
         void fund_rex_loan( T& table, const name& from, uint64_t loan_num, const asset& payment );
         template <typename T>
         void defund_rex_loan( T& table, const name& from, uint64_t loan_num, const asset& amount );
         void transfer_from_fund( const name& owner, const asset& amount );
         void transfer_to_fund( const name& owner, const asset& amount );
         bool rex_loans_available()const;
         bool rex_system_initialized()const { return _rexpool.begin() != _rexpool.end(); }
         bool rex_available()const { return rex_system_initialized() && _rexpool.begin()->total_rex.amount > 0; }
         static time_point_sec get_rex_maturity();
         asset add_to_rex_balance( const name& owner, const asset& payment, const asset& rex_received );
         asset add_to_rex_pool( const asset& payment );
         void process_rex_maturities( const rex_balance_table::const_iterator& bitr );
         void consolidate_rex_balance( const rex_balance_table::const_iterator& bitr,
                                       const asset& rex_in_sell_order );
         int64_t read_rex_savings( const rex_balance_table::const_iterator& bitr );
         void put_rex_savings( const rex_balance_table::const_iterator& bitr, int64_t rex );
         void update_rex_stake( const name& voter );

         void add_loan_to_rex_pool( const asset& payment, int64_t rented_tokens, bool new_loan );
         void remove_loan_from_rex_pool( const rex_loan& loan );
         template <typename Index, typename Iterator>
         int64_t update_renewed_loan( Index& idx, const Iterator& itr, int64_t rented_tokens );

         // defined in delegate_bandwidth.cpp
         void changebw( name from, const name& receiver,
                        const asset& stake_net_quantity, const asset& stake_cpu_quantity, bool transfer );
         void update_voting_power( const name& voter, const asset& total_update );

         // defined in voting.hpp
         void update_elected_producers( const block_timestamp& timestamp );
         void update_votes( const name& voter, const name& proxy, const std::vector<name>& producers, bool voting );
         void propagate_weight_change( const voter_info& voter );
         double update_producer_votepay_share( const producers_table2::const_iterator& prod_itr,
                                               const time_point& ct,
                                               double shares_rate, bool reset_to_zero = false );
         double update_total_votepay_share( const time_point& ct,
                                            double additional_shares_delta = 0.0, double shares_rate_delta = 0.0 );

         // defined in voting.cpp -> for witness pay
         double update_producer_witnesspay_share( const producers_table3::const_iterator& prod3_itr, time_point ct, double shares_rate, bool reset_to_zero = false );
         double update_total_witnesspay_share( time_point ct, double additional_shares_delta = 0.0, double shares_rate_delta = 0.0 );

         // defined in oracle.cpp
         void check_response_type(uint16_t t) const;
         std::pair<name, name> get_current_oracle() const;

         // defined in producer_pay.cpp
         void set_inflation_rate();
         void set_inflated_amount();
         void share_inflation();
         //void reset_bps_votes();
         void get_active_users();
         bool is_active_voter(name user);
         void payout_witness_reward();
         
         template <auto system_contract::*...Ptrs>
         class registration {
            public:
               template <auto system_contract::*P, auto system_contract::*...Ps>
               struct for_each {
                  template <typename... Args>
                  static constexpr void call( system_contract* this_contract, Args&&... args )
                  {
                     std::invoke( P, this_contract, args... );
                     for_each<Ps...>::call( this_contract, std::forward<Args>(args)... );
                  }
               };
               template <auto system_contract::*P>
               struct for_each<P> {
                  template <typename... Args>
                  static constexpr void call( system_contract* this_contract, Args&&... args )
                  {
                     std::invoke( P, this_contract, std::forward<Args>(args)... );
                  }
               };

               template <typename... Args>
               constexpr void operator() ( Args&&... args )
               {
                  for_each<Ptrs...>::call( this_contract, std::forward<Args>(args)... );
               }

               system_contract* this_contract;
         };

         registration<&system_contract::update_rex_stake> vote_stake_updater{ this };
   };

   /** @}*/ // end of @defgroup alaiosystem alaio.system
} /// alaiosystem

#pragma once
#include <alaio/asset.hpp>
#include <alaio/alaio.hpp>
#include <alaio/multi_index.hpp>
#include <alaio/crypto.hpp>
#include <alaio/transaction.hpp>

#include <stdlib.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <tuple>

const uint64_t FILEID_MULTIPPLIER = 0x100000000;

namespace alaio {
    using std::string;
    using std::tuple;
    using std::vector;
    using alaio::sha256;
    
    class [[alaio::contract("cryptlotto")]] cryptlotto : public contract {

        public:
            using contract::contract;
            
            cryptlotto( const name& self, const name& code, const datastream<const char*>& ds ): contract(self, code, ds) { }
            
            [[alaio::action]]
            void creategame( 
                const name& id,
                const string& title, 
                const string& description, 
                const string& image, 
                const uint64_t& reserved, 
                const uint64_t& ticket_limit, 
                const uint64_t& winners, 
                const time_point_sec& ends, 
                const asset& price,
                const vector<double>& percentages );

            [[alaio::action]]
            void updatetime( const name& id, const time_point_sec& ends );

            [[alaio::action]]
            void deletegame( const name& game );

            [[alaio::action]]
            void submitsecret( const name& user, const name& game, const string& secret );

            [[alaio::action]]
            void submithash( const name& user, const name& game, const checksum256& hash );
            
            [[alaio::on_notify("alaio.token::transfer")]]
            void purchase( const name& user, const name& to, const asset& quantity, const string& memo );

            [[alaio::action]]
            void getendgames( );
            
            [[alaio::action]]
            void emptytables( );

            [[alaio::action]]
            void cleanup( const name& game );

            [[alaio::action]]
            void revealwinner( const name& game );

        private:

            void refund_tickets( const name& game );
            
            void send_transfer( const name& from, const name& to, const asset& amount, const string& memo );

            void asset_valid( const asset& amount );

            void update_tree( const name& game, const asset& total, const name& user, const name& referrer );

            struct [[alaio::table("games")]] game {
                name            id;           /* autoincrement */
                string          title;
                string          description;
                string          image;

                uint64_t        reserved;
                uint64_t        ticket_limit;
                uint64_t        winners;

                time_point_sec  ends;
                asset           price;
                asset           winnings;
                
                uint64_t primary_key() const { return id.value; }
                uint64_t get_ends() const { return ends.utc_seconds; }
            };

            struct [[alaio::table("tickets")]] ticket {
                uint64_t     id;
                name         user;
                string       secret;
                checksum256  hash;

                uint64_t primary_key() const { return id; }
                uint64_t get_user() const { return user.value; }
            };

            struct [[alaio::table("referrals")]] referral {
                name      user;
                uint64_t  treepos;
                uint64_t  referrals;

                uint64_t primary_key() const { return user.value; }
                uint64_t get_treepos() const { return treepos; }
                uint64_t get_referrals() const { return referrals; }
            };

            struct [[alaio::table("referrers")]] referrers {
                uint64_t id;
                name user;
                name referrer;

                uint64_t primary_key() const { return id; }
                uint64_t get_user() const { return user.value; }
                uint64_t get_referrer() const { return referrer.value; }
            };

            struct [[alaio::table("hashes")]] hash {
                name         user;
                checksum256  hash;

                uint64_t primary_key() const { return user.value; }
            };

            struct [[alaio::table("winpercent")]] percentages {
                uint64_t id;
                double   percent;

                uint64_t primary_key() const { return id; }
            };

            struct [[alaio::table]] currency_stats {
                asset    supply;
                asset    max_supply;
                name     issuer;

                uint64_t primary_key()const { return supply.symbol.code().raw(); }
            };
            
            typedef struct player_ticket {
                uint64_t ticket_id;
                name user;
                string secret;
                checksum256 hash;
            } player_ticket;

            typedef struct winner {
                uint64_t rasult;
                uint64_t winner;
            } winner;

            typedef alaio::multi_index< "games"_n, game, indexed_by< "ending"_n, const_mem_fun<game, uint64_t, &game::get_ends > > > games_index;
            
            typedef alaio::multi_index< "tickets"_n, ticket, indexed_by< "byuser"_n, const_mem_fun< ticket, uint64_t, &ticket::get_user > > > tickets_index;

            typedef alaio::multi_index< "referrals"_n, referral, indexed_by< "byreferrals"_n, const_mem_fun< referral,uint64_t, &referral::get_referrals > >,indexed_by< "bytree"_n,const_mem_fun<referral,uint64_t,&referral::get_treepos>>> referrals_index;

            typedef alaio::multi_index< "referrers"_n, referrers, indexed_by< "byuser"_n, const_mem_fun< referrers,uint64_t, &referrers::get_user > >, indexed_by< "byreferrer"_n, const_mem_fun< referrers,uint64_t, &referrers::get_referrer > > > referrers_index;

            typedef alaio::multi_index< "hashes"_n, hash > game_hashes;

            typedef alaio::multi_index< "winpercent"_n, percentages > winner_percentage;

            typedef alaio::multi_index< "stat"_n, currency_stats > stats;

    };
}
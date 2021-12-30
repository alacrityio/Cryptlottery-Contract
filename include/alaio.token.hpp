#pragma once

#include <alaio/asset.hpp>
#include <alaio/alaio.hpp>
#include <alaio/system.hpp>

#include <string>
#include <cmath>

#include <alaio.system/alaio.system.hpp>

namespace alaiosystem {
   class system_contract;
}

namespace alaio {

   using std::string;
   using alaio::current_time_point;

   /**
    * @defgroup alaiotoken alaio.token
    * @ingroup alaiocontracts
    *
    * alaio.token contract
    *
    * @details alaio.token contract defines the structures and actions that allow users to create, issue, and manage
    * tokens on alaio based blockchains.
    * @{
    */
   class [[alaio::contract("alaio.token")]] token : public contract {
      public:
         using contract::contract;

         /**
          * Create action.
          *
          * @details Allows `issuer` account to create a token in supply of `maximum_supply`.
          * @param issuer - the account that creates the token,
          * @param maximum_supply - the maximum supply set for the token created.
          *
          * @pre Token symbol has to be valid,
          * @pre Token symbol must not be already created,
          * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 1^62 - 1.
          * @pre Maximum supply must be positive;
          *
          * If validation is successful a new entry in statstable for token symbol scope gets created.
          */
         [[alaio::action]]
         void create( const name&   issuer,
                      const asset&  maximum_supply);
         /**
          * Issue action.
          *
          * @details This action issues to `to` account a `quantity` of tokens.
          *
          * @param to - the account to issue tokens to, it must be the same as the issuer,
          * @param quntity - the amount of tokens to be issued,
          * @memo - the memo string that accompanies the token issue transaction.
          */
         [[alaio::action]]
         void issue( const name& to, const asset& quantity, const string& memo );

         /**
          * Retire action.
          *
          * @details The opposite for create action, if all validations succeed,
          * it debits the statstable.supply amount.
          *
          * @param quantity - the quantity of tokens to retire,
          * @param memo - the memo string to accompany the transaction.
          */
         [[alaio::action]]
         void retire( const asset& quantity, const string& memo );

         /**
          * Transfer action.
          *
          * @details Allows `from` account to transfer to `to` account the `quantity` tokens.
          * One account is debited and the other is credited with quantity tokens.
          *
          * @param from - the account to transfer from,
          * @param to - the account to be transferred to,
          * @param quantity - the quantity of tokens to be transferred,
          * @param memo - the memo string to accompany the transaction.
          */
         [[alaio::action]]
         void transfer( const name&    from,
                        const name&    to,
                        const asset&   quantity,
                        const string&  memo );

         [[alaio::action]]
         void applyfee( const name& to, const asset& quantity, const string&  memo);
         /**
          * Open action.
          *
          * @details Allows `ram_payer` to create an account `owner` with zero balance for
          * token `symbol` at the expense of `ram_payer`.
          *
          * @param owner - the account to be created,
          * @param symbol - the token to be payed with by `ram_payer`,
          * @param ram_payer - the account that supports the cost of this action.
          *
          * More information can be read [here](https://github.com/ALAIO/alaio.contracts/issues/62)
          * and [here](https://github.com/ALAIO/alaio.contracts/issues/61).
          */
         [[alaio::action]]
         void open( const name& owner, const symbol& symbol, const name& ram_payer );

         /**
          * Close action.
          *
          * @details This action is the opposite for open, it closes the account `owner`
          * for token `symbol`.
          *
          * @param owner - the owner account to execute the close action for,
          * @param symbol - the symbol of the token to execute the close action for.
          *
          * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
          * @pre If the pair of owner plus symbol exists, the balance has to be zero.
          */
         [[alaio::action]]
         void close( const name& owner, const symbol& symbol );

         //altcoin

         [[alaio::action]]
         void update(
            const string tokenSymbol,
            const string tokenName,
            const string description,
            const string type,    //utility or security            
            const string api_url,
            const double fee,
            const string website);

         void addUnpaidFee(asset fee);
         
         void applyUnpaidFees(symbol sym);

         /**
          * Get supply method.
          *
          * @details Gets the supply for token `sym_code`, created by `token_contract_account` account.
          *
          * @param token_contract_account - the account to get the supply for,
          * @param sym_code - the symbol to get the supply for.
          */
         static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         /**
          * Get balance method.
          *
          * @details Get the balance for a token `sym_code` created by `token_contract_account` account,
          * for account `owner`.
          *
          * @param token_contract_account - the token creator account,
          * @param owner - the account for which the token balance is returned,
          * @param sym_code - the token for which the balance is returned.
          */
         static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }

         static asset get_balance_or_zero( name token_contract_account, name owner, symbol sym )
         {
            accounts accountstable( token_contract_account, owner.value );
            auto it = accountstable.find( sym.code().raw() );
            if( it == accountstable.end() ) {
               return asset{0, sym};
            } else {
               return it->balance;
            }         
         }

         static string trim(const string& s) {
            string WHITESPACE = " \n\r\t\f\v";
            //ltrim
            size_t start = s.find_first_not_of(WHITESPACE);
            string ss = (start == std::string::npos) ? "" : s.substr(start);
            //rtrim          
            size_t end = ss.find_last_not_of(WHITESPACE);
	         string sss = (end == std::string::npos) ? "" : ss.substr(0, end + 1);
            return sss;
         }

         //static time_point current_time_point() {
         //   const static time_point ct{ microseconds{ static_cast<int64_t>( current_time() ) } };
         //   return ct;
         //}

         using create_action = alaio::action_wrapper<"create"_n, &token::create>;
         using issue_action = alaio::action_wrapper<"issue"_n, &token::issue>;
         using retire_action = alaio::action_wrapper<"retire"_n, &token::retire>;
         using transfer_action = alaio::action_wrapper<"transfer"_n, &token::transfer>;
         using open_action = alaio::action_wrapper<"open"_n, &token::open>;
         using close_action = alaio::action_wrapper<"close"_n, &token::close>;
         using update_action = alaio::action_wrapper<"update"_n, &token::update>;
      private:
         struct [[alaio::table]] account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[alaio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         struct [[alaio::table]] currency_metadata {
            string      tokenSymbol;
            string      tokenName;
            string      type;          //utility or security
            string      description;            
            string      api_url;
            string      website;
            double      fee;
            uint64_t    holders;
            time_point  last_update;

            uint64_t primary_key() const { return symbol_code(tokenSymbol).raw(); }
         };

         struct [[alaio::table]] currency_fees {
            asset    unpaid_fees;

            uint64_t primary_key()const { return unpaid_fees.symbol.code().raw(); }
         };

         typedef alaio::multi_index< "accounts"_n, account > accounts;
         typedef alaio::multi_index< "stat"_n, currency_stats > stats;
         typedef alaio::multi_index< "metadata"_n, currency_metadata > metadata;
         typedef alaio::multi_index< "fees"_n, currency_fees > fees;


         void sub_balance( const name& owner, const asset& value );
         void add_balance( const name& owner, const asset& value, const name& ram_payer );
   };
   /** @}*/ // end of @defgroup alaiotoken alaio.token
} /// namespace alaio

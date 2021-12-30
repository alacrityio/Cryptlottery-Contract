#include <cryptlotto.hpp>

const float TREE_PERCENT = 0.05;
const float FEE_PERCENT = 0.10;
const float REFERRAL_PERCENT = 0.05;

namespace alaio {

    void cryptlotto::creategame( 
                const name& id,
                const string& title, 
                const string& description, 
                const string& image, 
                const uint64_t& reserved, 
                const uint64_t& ticket_limit, 
                const uint64_t& winners, 
                const time_point_sec& ends, 
                const asset& price,
                const vector<double>& percentages) {
        require_auth( get_self() );
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(id.value);
        check(found_game == games.end(), "Game with id exists");
            auto sym = price.symbol;
            asset_valid(price);
            check( price.amount > 0, "price must be greater than 0" );

            auto _now = time_point_sec(current_time_point());
            check(ends > _now, "ending cant be before now");

            asset winnings;
            winnings.amount = 0;
            winnings.symbol = sym;

            
            games.emplace(get_self(), [&](auto& row) {
                row.id = id;
                row.title = title;
                row.description = description;
                row.image = image;
                row.reserved = reserved;
                row.ticket_limit = ticket_limit;
                row.winners = winners;
                row.price = price;
                row.ends = ends;
                row.winnings = winnings;
            });

            winner_percentage percentage_index(get_self(), id.value);
            for(auto it = percentages.begin(); it != percentages.end(); it++) {
                percentage_index.emplace(get_self(), [&](auto& row) {
                    row.id = percentage_index.available_primary_key();
                    row.percent = *it;
                });
            }
        
    }

    void cryptlotto::updatetime( const name& id, const time_point_sec& ends ) {
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(id.value);
        check(found_game != games.end(), "Game does not exsist");
        games.modify(found_game, get_self(), [&](auto& row) {
            row.ends = ends;
        });
    }

    void cryptlotto::deletegame( const name& game ) {
        require_auth( get_self() );
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(game.value);
        check(found_game != games.end(), "game does not exist");
        cleanup(found_game->id);
        games.erase(found_game);
    }

    void cryptlotto::refund_tickets( const name& game ) {

    }

    void cryptlotto::submithash( const name& user, const name& game, const checksum256& hash ) {
        require_auth(user);
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(game.value);
        check(found_game != games.end(), "game does not exist");
        game_hashes hashes(get_self(), game.value);
        auto iterator = hashes.find(user.value);
        check( iterator == hashes.end(), "Cannot resubmit hash");
        // no user hash for game
        hashes.emplace(user, [&](auto& row) {
            row.user = user;
            row.hash = hash;
        });
        print("submitted hash");
    }

    void cryptlotto::purchase( const name& user, const name& to, const asset& quantity, const string& memo ) {
        if (user == get_self() || to != get_self()){ return; }
        string str = memo;
        string game;
        string referrer;
        
        std::size_t split = memo.find(" ");
        if(split != string::npos) {
            game = memo.substr(0, str.find(" "));
            referrer = str.substr(split+1, str.length());
        } else {
            game = memo;
            referrer = "";
        }
        
        print("quantity from alacrity, ", quantity, "\n");
        // check if game exsists
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(name(game).value);
        check(found_game != games.end(), "game does not exist");

        tickets_index tickets(get_self(), name(game).value);
        auto reserve = tickets.find(found_game->reserved - 1);
        //print("last ticket", last_ticket->id);

        // check if game has ended
        auto now = time_point_sec(current_time_point()).utc_seconds;
        check(found_game->ends.utc_seconds > now || reserve == tickets.end(), "Game has ended");

        // check for valid payment and if payment creates whole number for amount of tickets to buy
        check(quantity.symbol == found_game->price.symbol, "Wrong currency for game");
        check(quantity.amount % found_game->price.amount == 0, "Amount not divisable by game price" );

        // check if user has submitted their secret
        game_hashes hashes(get_self(), name(game).value);
        auto secret_hash = hashes.find(user.value);
        check(secret_hash != hashes.end(), "submit hash first");

        if(found_game->ticket_limit > 0){
            tickets_index tickets(get_self(), name(game).value);
            auto tickiter = tickets.find(found_game->ticket_limit - 1);
            check(tickiter == tickets.end() , "Game is sold out");

            tickiter = tickets.find(found_game->ticket_limit - ((quantity.amount / found_game->price.amount)));
            check(tickiter == tickets.end() && found_game->ticket_limit >= (quantity.amount / found_game->price.amount) , "Cant Buy that many tickets");
        }

        // update tree and pay out referrals
        print("Update Tree \n");
        update_tree(name(game), quantity, user, name(referrer));

        // calculate asset to add to winnings
        asset after_fees;
        after_fees.amount = quantity.amount - (quantity.amount * (REFERRAL_PERCENT + FEE_PERCENT + TREE_PERCENT));
        after_fees.symbol = quantity.symbol;

        print("after fees, ", after_fees, "\n");
        games.modify(found_game, get_self(), [&](auto& row) {
            row.winnings += after_fees;
        });

        // give player tickets
        for(int i = 0; i < quantity.amount / found_game->price.amount; i++) {
            tickets.emplace(get_self(), [&](auto& row) {
                row.id = tickets.available_primary_key();
                row.user = user;
                row.hash = secret_hash->hash;
            });
        }

        // delete hash from table
        hashes.erase(secret_hash);
    }

    void cryptlotto::submitsecret(const name& user, const name& game, const string& secret) {
        check(has_auth(user) || has_auth(get_self()), "Only the ticket owner or contract can submit secret for this user");

        auto now = time_point_sec(current_time_point()).utc_seconds;
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(game.value);
        check(found_game->ends.utc_seconds < now, "Game has not ended yet");
        
        tickets_index tickets(get_self(), game.value);
        auto reserve = tickets.find(found_game->reserved - 1);
        check(reserve != tickets.end(), "Game Reserve not Met");

        auto user_index = tickets.get_index<"byuser"_n>();
        auto user_tickets = user_index.find(user.value);
        check(user_tickets != user_index.end(), "no tickets for user");

        checksum256 submitted_secret = sha256( (char *)secret.c_str(), secret.size() );
        bool found_match = false;
        for(auto i = user_index.begin(); i != user_index.end(); i++) {
            if(i->hash == submitted_secret) {
                user_index.modify(i, get_self(), [&](auto& row) {
                    row.secret = secret;
                });
                found_match = true;
            }
        }
        check(found_match, "No matching commitment found for this secret");
    }

    void cryptlotto::emptytables() {
        require_auth( get_self() );
        games_index games(get_self(), get_first_receiver().value);
        auto gameitr = games.begin();
        // loop through all games
        while(gameitr != games.end()) {
            gameitr = games.erase(gameitr);

            cleanup(gameitr->id);
        }
    }

    void cryptlotto::cleanup(const name& game) {
        tickets_index tickets(get_self(), game.value);
        auto tickiter = tickets.begin();
        while(tickiter != tickets.end()) {
            tickiter = tickets.erase(tickiter);
        }

        game_hashes hashes(get_self(), game.value);
        auto hashiter = hashes.begin();
        while(hashiter != hashes.end()) {
            hashiter = hashes.erase(hashiter);
        }

        winner_percentage perc(get_self(), game.value);
        auto piter = perc.begin();
        while(piter != perc.end()) {
            piter = perc.erase(piter);
        }
    }

    void cryptlotto::getendgames( ) {
        require_auth( get_self() );
        auto now = time_point_sec(current_time_point()).utc_seconds;
        games_index games(get_self(), get_self().value);
        auto gameitr = games.begin();

        

        while(gameitr != games.end()) {

            if(gameitr->ends.utc_seconds < now) {
                revealwinner(gameitr->id);
                gameitr = games.erase(gameitr);
            } else {
                gameitr++;
            }
        }
    }

    void cryptlotto::revealwinner( const name& game ) {
        require_auth( get_self() );
        games_index games(get_self(), get_self().value);
        auto found_game = games.find(game.value);
        check(found_game != games.end(), "game does not exist");

        print("reveal ", game, "\n");

        tickets_index tickets(get_self(), game.value);
        uint64_t ticket_count = 0;
        if(tickets.begin() != tickets.end()) {
            uint32_t result_value = 0;
            for(auto i = tickets.begin(); i != tickets.end(); i++) {
                ticket_count++;
                if(i->secret.size() > 0) {
                    player_ticket ticket = { i->id, i->user, i->secret, i->hash };
                    checksum256 result = sha256( (char *)&ticket, sizeof(ticket) );
                    auto hash_result = result.extract_as_byte_array();
                    result_value += hash_result[0];
                }
            }
            print("Ticket Count: ", ticket_count, "\n");
            check(result_value > 0, "No commitment reveals, uh oh \n");
            winner_percentage perc(get_self(), game.value);
            auto piter = perc.begin();
            print("result", result_value, "\n");
            for(uint64_t winnerint = 0; winnerint < found_game->winners; winnerint ++) {
                asset winnings = found_game->winnings;
                winnings.amount = winnings.amount * piter->percent;
                winner result_ticket = { result_value, winnerint };
                checksum256 result = sha256( (char *)&result_ticket, sizeof(result_ticket) );
                auto hash_result = result.extract_as_byte_array();
                uint64_t ticket = hash_result[0];
                auto winning_ticket = tickets.find(ticket % ticket_count);
                print("Val: ", ticket, ", Winning Ticket: ", ticket % ticket_count,  ", Winner: ", winning_ticket->user, "\n");
                send_transfer(get_self(), winning_ticket->user, winnings, game.to_string() + " Winner of Lotto");
                piter++;

            }
            
            // cleanup(game);
        } else {
            print("No tickets sold wah wah wah");
        }
    }

    void cryptlotto::update_tree( const name& game, const asset& total, const name& user, const name& referrer) {
        // tickets table
        tickets_index tickets(get_self(), game.value);
        auto users_index = tickets.get_index<"byuser"_n>();
        auto found_user = users_index.find(referrer.value);

        if(found_user != users_index.end() && user != referrer) {
            print("referral ", referrer, " found in tickets \n");
            bool valid_referral = true;

            referrers_index referrers(get_self(), game.value);

            auto refer_user_index = referrers.get_index<"byuser"_n>();
            auto referrer_referrers = refer_user_index.find(user.value);
            auto referrer_index = referrers.get_index<"byreferrer"_n>();
            auto referrer_refer = referrer_index.find(referrer.value);

            if(referrer_refer != referrer_index.end()) {
                print(user, " has been referred by ", referrer, "\n");
                valid_referral = false;
            } else {
                print(user, " has not been referred by ", referrer, "\n");
            }

            auto user_refer_index = referrers.get_index<"byuser"_n>();
            auto user_referrers = user_refer_index.find(referrer.value);
            auto user_index = referrers.get_index<"byreferrer"_n>();
            auto user_refer = user_index.find(user.value);
            
            if(user_refer != user_index.end()) {
                print(referrer, " has referred by ", user, "\n");
                valid_referral = false;
            } else {
                print(referrer, " has not referred by ", user, "\n");
                
            }

            asset referral_reward;
            referral_reward.amount = total.amount * REFERRAL_PERCENT;
            referral_reward.symbol = total.symbol;
            send_transfer(get_self(), name(referrer), referral_reward, "Referral Reward");

            // user found in tickets increment or add user to referrals 
            referrals_index referrals(get_self(), game.value);
            auto referral = referrals.find(found_user->user.value);
            
            if(valid_referral) {
                auto tree_index = referrals.get_index<"bytree"_n>();
                print("Last TreePos,  ", tree_index.end()->treepos, "\n");
                print("First TreePos, ", tree_index.begin()->treepos, "\n");
                if(referral != referrals.end()) {
                    uint64_t treepos = 0;
                    if(referral->treepos == 0) {
                        treepos = tree_index.begin()->treepos + 1;
                    } else {
                        treepos = referral->treepos;
                    }
                    referrals.modify(referral, get_self(), [&](auto& row) {
                        row.treepos = tree_index.begin()->treepos + 1;
                        row.referrals += 1;
                    });
                } else {
                    referrals.emplace(get_self(), [&](auto& row) {
                        row.user = referrer;
                        row.treepos = 0;
                        row.referrals = 1;
                    });
                }
                referrers.emplace(get_self(), [&](auto& row) {
                    row.id = referrers.available_primary_key();
                    row.user = user;
                    row.referrer = referrer;
                });
            }

            referral = referrals.find(found_user->user.value);
            auto referral_index = referrals.get_index<"bytree"_n>();
            auto applicable_referrals = referral_index.upper_bound(referral->treepos);

            if(applicable_referrals != referral_index.end()) {
                
                uint64_t applicable_players = 0;
                while(applicable_referrals != referral_index.end()) {
                    applicable_referrals++;
                    applicable_players++;
                }
                print(applicable_players, " players in tree \n");
                asset tree_reward;
                tree_reward.amount = total.amount * TREE_PERCENT / applicable_players;
                tree_reward.symbol = total.symbol;

                applicable_referrals = referral_index.upper_bound(referral->treepos);
                for(auto referral = applicable_referrals; referral != referral_index.end(); referral ++) {
                    print("send tree ", tree_reward, " to ", referral->user);
                    send_transfer(get_self(), referral->user, tree_reward, "Tree Reward");
                }
            } else {
                print("no applicable referrals \n");
            }
        } else {
            print(referrer, " not found in tickets \n");
        }

        
    }

    void cryptlotto::asset_valid( const asset& amount ) {
        check( amount.symbol.is_valid(), "invalid symbol name" );
        check( amount.is_valid(), "invalid price" );
        stats statstable( name("alaio.token"), amount.symbol.code().raw() );
        auto existing = statstable.find( amount.symbol.code().raw() );
        check( existing != statstable.end(), "token with symbol does not exsist" );
    }

    void cryptlotto::send_transfer( const name& from, const name& to, const asset& amount, const string& memo ) {
        action(
            permission_level(get_self(), "active"_n),
            "alaio.token"_n,
            "transfer"_n,
            make_tuple(from, to, amount, memo)
        ).send();
    }

}
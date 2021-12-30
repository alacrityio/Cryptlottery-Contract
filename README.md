Helpfull commands

generate hash
`echo -n 'mysecret' | xxd -r -p | sha256sum -b | awk '{print $1}'`

submit hash
`cleos push action cryptlotto submithash '["kyle", 0, "kyle secret new", "f826446dbf87dbe123a6af5a76fb8136bd163505c05825cfdcbae3b04fd1b428"]' -p kyle@active`
`cleos push action cryptlotto submithash '["kyle", 0, "nicks secret", "c6b3a21fd09cd825c536c829380ce5073d6e01bcfba10717f373e956e730b240"]' -p nick@active`

submit secret
`cleos push action cryptlotto submitsecret '["kyle", "game_id", "secret"]' -p kyle@active`

create game
`cleos push action cryptlotto creategame '["test Game", "rand description", "2020-09-08T00:00:00", "0.0500 SYS"]' -p cryptlotto@active`

purchase ticket
`cleos push action eosio.token transfer '["nick", "cryptlotto", "10.0000 SYS", "0 kyle"]' -p nick@active`

reveal winner
`cleos -v push action cryptlotto revealwinner '[0]' -p cryptlotto@active`

get account balance
`alacli get currency balance alaio.token cryptlottery ALA`

issue tokens
`cleos push action eosio.token issue '[ "nick", "100.0000 SYS", "memo" ]' -p nick@active`

cryptlottery commands

submit hash
`alacli push action cryptlottery submithash '["lizardking", "pahfcdeip", "c6b3a21fd09cd825c536c829380ce5073d6e01bcfba10717f373e956e730b240"]' -p lizardking@active`
`alacli push action cryptlottery submithash '["eraguth", "pacfyeghu", "c6b3a21fd09cd825c536c829380ce5073d6e01bcfba10717f373e956e730b240"]' -p eraguth@active`

purchase ticket
`alacli -v push action alaio.token transfer '["lizardking", "cryptlottery", "1.0000 ALA", "pahfcdeip"]' -p lizardking@active`
`alacli -v push action alaio.token transfer '["eraguth", "cryptlottery", "2.0000 ALA", "pacfyeghu lizardking"]' -p eraguth@active`

cleanup
`alacli push action cryptlottery cleanup '[0]' -p cryptlottery@active`

reveal winner
`alacli push action cryptlottery revealwinner '["1eh5.3da"]' -p cryplottery@active`

clear
alacli wallet unlock --password PW5JmXLwuGcYv1nYB8tyMADKCCGcFqgvDwNzMmYEFnn5NxpZKs12e
alacli push action cryptlottery deletegame '["paiegghfa"]' -p cryptlottery@active


https://local.bloks.io/?nodeUrl=api.testnet.eos.io&systemDomain=eosio&hyperionUrl=https%3A%2F%2Fjungle3history.cryptolions.io
"# Cryptlottery-Contract" 

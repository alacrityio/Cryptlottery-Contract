<h1 class="contract">creategame</h1>
---
spec-version: 0.0.2
title: Create Game
summary: When this action is called by the contract owner it will create a game. the ending time cannot be before now UTC and price should match asset you want to the game to be played in e.g: price -> "0.0500 SYS".
icon: 

<h1 class="contract">deletegame</h1>
---
spec-version: 0.0.2
title: Delete Game
summary: When this action is called by the contract owner it will delete the game and refund tickets to the users.
icon: 

<h1 class="contract">submithash</h1>
---
spec-version: 0.0.2
title: Submit Hash
summary: The user must call this action fist to submit a secret and hash, then they can purchase tickets in which the secret and hash is assigned to those tickets. I a user wants to buy more tickets after the fact they have to re submit a secret and hash, can be same set or different.
icon: 

<h1 class="contract">getendgames</h1>
---
spec-version: 0.0.2
title: Get Ending Games
summary: This action when called with the contract account will Get all games past their ending cryteria and reveal winner of the games and pay them their winnings. the winner of the game is calculated by sha256({ticket_id, user, secret, hash}, length).
icon: 

<h1 class="contract">submitsecret</h1>
---
spec-version: 0.0.2
title: Submit Secret
summary: Submit your secret to allow the contract to use your ticket in the RNG process.
icon: 

<h1 class="contract">emptytables</h1>
---
spec-version: 0.0.2
title: Empty Games Table tickets and hashes
summary: For testing Empty games table
icon: 
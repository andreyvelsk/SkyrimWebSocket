# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [1.7.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.6.0...v1.7.0) (2026-04-23)


### ✨ Features

* **hotkey:** add extra fields to hotkey type ([91523c2](https://github.com/yourusername/SkyrimWebSocket/commits/91523c2d226e6c1398e8ce11b6b7e393b2a89e7b))
* **hotkey:** native trigger ([f9a514f](https://github.com/yourusername/SkyrimWebSocket/commits/f9a514fd5dbe7c9fafd6dc6ff1031afdd2f9dfd0))
* **hotkeys:** real ([cde18a2](https://github.com/yourusername/SkyrimWebSocket/commits/cde18a2a2745ed1cd4752336b4b657e87f170109))


### 🐛 Bug Fixes

* **build:** lost header ([46ab23e](https://github.com/yourusername/SkyrimWebSocket/commits/46ab23e140aeeb1a753bf8933fc07f9e7adf9693))
* **hotkey:** build ([e72323f](https://github.com/yourusername/SkyrimWebSocket/commits/e72323f843b4fda4d5fec4ea8ded60a8cb466778))

## [1.6.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.2.0...v1.6.0) (2026-04-21)


### ✨ Features

* add description field with <mag>/<dur> substituted in effect entries ([e74c24d](https://github.com/yourusername/SkyrimWebSocket/commits/e74c24d710bd29295ae744edcf25c587e2a01411))
* add Game::Language field to retrieve current game language ([a6f6390](https://github.com/yourusername/SkyrimWebSocket/commits/a6f6390ba4f42d9c5b45f534cca4564a88b404fc))
* add JSON inventory fields to FieldRegistry with mixed-type support ([f6abe65](https://github.com/yourusername/SkyrimWebSocket/commits/f6abe65c1399c3a939e30f423983d4e154d371dc))
* add Player::Level, Player::XP::*, Player::InventoryWeight, Player::CarryWeight fields ([16a205a](https://github.com/yourusername/SkyrimWebSocket/commits/16a205a5c432234ed20846aa4d1c9ac1726ccec6))
* **ammo:** add new fields ([12db9e1](https://github.com/yourusername/SkyrimWebSocket/commits/12db9e1ed0f6c6c79beb21fcc70dc95d5c8f7d6e))
* enrich inventory data — enchantment details, body slots, damage formula, food, books, soul gem fill state, effect descriptions ([5e3e91e](https://github.com/yourusername/SkyrimWebSocket/commits/5e3e91e92844d4381618e24a962630759a2d1f96))
* expand inventory item data (equipped, favorite, descriptions, enchantments, etc.) ([50aeb34](https://github.com/yourusername/SkyrimWebSocket/commits/50aeb3432e908952127dd38a2434a01462e89014))
* inventory command system (equip/unequip/use/drop/favorite) ([#18](https://github.com/yourusername/SkyrimWebSocket/issues/18)) ([31873fc](https://github.com/yourusername/SkyrimWebSocket/commits/31873fc4212b24ec4b71fb0f95e39f7cb9e7a971))
* **inventory:** add categoryType field ([1fd492b](https://github.com/yourusername/SkyrimWebSocket/commits/1fd492b3a72e6dd8f1f0f0d6684197c45cd6419f))
* localized templates, isStolen, armorTypeId/armorType, categoryId, no hardcoded strings ([a989c21](https://github.com/yourusername/SkyrimWebSocket/commits/a989c2190b60dc671e52950b287f4b839f8d64ba))
* **magic:** add spells to favorite ([c85baa8](https://github.com/yourusername/SkyrimWebSocket/commits/c85baa8561cb5d3f859090f2443ca4afb56621a6))
* **magic:** equip spell command ([ca331a9](https://github.com/yourusername/SkyrimWebSocket/commits/ca331a9f4d375b5a5cdecf10f2ed1ce8a5eb999e))
* **maigc:** magic categories ([3185ec4](https://github.com/yourusername/SkyrimWebSocket/commits/3185ec4433a18235b1185086050a8ac841110609))
* remove describe type, add heartbeat message type ([6cbdc2c](https://github.com/yourusername/SkyrimWebSocket/commits/6cbdc2c5929729a890249b8968c154df545757cf))
* require id in query; add unsubscribe_all ([eaaa9ff](https://github.com/yourusername/SkyrimWebSocket/commits/eaaa9ff3971ba65740da6f315d189bd25bd38e24))
* support multiple named subscriptions per connection ([7030a3a](https://github.com/yourusername/SkyrimWebSocket/commits/7030a3ae5780bde267b9f8cfed49cafcd7b67182))


### 🐛 Bug Fixes

* access PlayerCharacter skills via GetInfoRuntimeData() for CommonLibSSE-NG multi-targeting ([e690709](https://github.com/yourusername/SkyrimWebSocket/commits/e6907097b4d03a28b5cbe027e2839394d1881823))
* address code review feedback in InventoryReader ([b909a39](https://github.com/yourusername/SkyrimWebSocket/commits/b909a397e641115bd4d4ca5fcc86f56e8acf2f62))
* create ExtraDataList for items without one when favoriting ([45abb76](https://github.com/yourusername/SkyrimWebSocket/commits/45abb762c77125fb42d8e123ac2762ae05d39dd0))
* **inventory:** build bugfix ([b5b2e5d](https://github.com/yourusername/SkyrimWebSocket/commits/b5b2e5d26c0cbcee2832660d5bed1f89234e821b))
* **inventory:** build fix ([339031f](https://github.com/yourusername/SkyrimWebSocket/commits/339031f5d19d1414775ece5730cbc9502acc3eaf))
* **magic:** add debug to find reason of unequip not working ([04bba9f](https://github.com/yourusername/SkyrimWebSocket/commits/04bba9f88a7fde97d232d561ce3639a11daf46c0))
* **magic:** check for equiped hand ([de71ab7](https://github.com/yourusername/SkyrimWebSocket/commits/de71ab72d279b844f8e90db4abf186e06c074e1b))
* **magic:** deselect spell ([b044dbd](https://github.com/yourusername/SkyrimWebSocket/commits/b044dbd0699c37a37f42ecb87ec6695c4f1ab4c5))
* **magic:** deselect spell for both hands bug ([c0715eb](https://github.com/yourusername/SkyrimWebSocket/commits/c0715eb340a517fc195aa6c3cc073f1988e6a0ff))
* **magic:** fix player known spell by equip command ([babc4b0](https://github.com/yourusername/SkyrimWebSocket/commits/babc4b0f5739635b53212147ce94df55fb6ec457))
* **magic:** level, right cost, unequip ([062bfdb](https://github.com/yourusername/SkyrimWebSocket/commits/062bfdbb1be706c1bcb4aeac41ca08e1b896f16b))
* **magic:** more spells of school reading ([c4bc962](https://github.com/yourusername/SkyrimWebSocket/commits/c4bc9629a7d4941c7513f875ace43081966e95e4))
* **magic:** wrong function call ([d20a8bd](https://github.com/yourusername/SkyrimWebSocket/commits/d20a8bd6c0b63128a940cb2e8d0768ab97f1ce23))
* **magix:** build error ([3e5bb03](https://github.com/yourusername/SkyrimWebSocket/commits/3e5bb0375a07cf04af9d80cbd6545faf0ccdb444))
* **maigc:** unequip master spell ([100fbd6](https://github.com/yourusername/SkyrimWebSocket/commits/100fbd6560ea5c0b6f96a1f7882b805397eaae72))
* read descriptionTemplate from magicItemDescription (DNAM), not TESDescription ([cb6315c](https://github.com/yourusername/SkyrimWebSocket/commits/cb6315c4e0b22617d5514c76c2502e8e23cba8eb))
* **release:** fix npm release bug ([341d4c1](https://github.com/yourusername/SkyrimWebSocket/commits/341d4c1ee64aa3fefc150d911c0762ea0b4a5275))
* remove const on TESDescription/TESObjectARMO pointers to fix C2662 build errors ([8cb4f06](https://github.com/yourusername/SkyrimWebSocket/commits/8cb4f061ee6569d6d800e332cfb0cda8407fd3a6))
* remove misleading GMST mappings for categories; fix PROTOCOL.md example ([63e1990](https://github.com/yourusername/SkyrimWebSocket/commits/63e1990348c0c0f592dc417dce02c0d1b527ed98))
* replace non-existent ExtraStolenFlag with ExtraOwnership check ([fab0d61](https://github.com/yourusername/SkyrimWebSocket/commits/fab0d61379e8545d64ef995519ca75797cfa4f04))
* **semantic:** corerct change release version ([6e29c29](https://github.com/yourusername/SkyrimWebSocket/commits/6e29c29899874d3d30959b8c28d2c3f8390ceb57))
* static_cast for descriptionTemplate + add baseArmorRating field ([a76cdb9](https://github.com/yourusername/SkyrimWebSocket/commits/a76cdb901fda9d868aae816327a17c114713fba4))
* use correct RE::FormType enum values (no k prefix) for CommonLibSSE ([4040110](https://github.com/yourusername/SkyrimWebSocket/commits/40401107c808637e42e4a3d4fa99daaa3ff92552))
* use engine SetFavorite/RemoveFavorite instead of manual ExtraDataList construction ([5b3f73b](https://github.com/yourusername/SkyrimWebSocket/commits/5b3f73b055a6e04a9e3fa78f67cff77ae0ce2ce2))

### [1.0.1](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.0.1) (2026-04-07)


### ✨ Features

* add Base and Clamped ActorValue getter ([7ffbf0a](https://github.com/yourusername/SkyrimWebSocket/commits/7ffbf0af98c13c59d878cdd36241140b370da43c))
* enhance FieldRegistry to support current and permanent value types ([002f1ba](https://github.com/yourusername/SkyrimWebSocket/commits/002f1ba95142697d4c9291a8830d8428b463a8dc))


### 🐛 Bug Fixes

* restrict build workflow to main branch only ([e1a445f](https://github.com/yourusername/SkyrimWebSocket/commits/e1a445f30687b1090557af6018e8aacb8ddfe24a))

## [1.5.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.2.0...v1.5.0) (2026-04-20)


### ✨ Features

* add description field with <mag>/<dur> substituted in effect entries ([e74c24d](https://github.com/yourusername/SkyrimWebSocket/commits/e74c24d710bd29295ae744edcf25c587e2a01411))
* add Game::Language field to retrieve current game language ([a6f6390](https://github.com/yourusername/SkyrimWebSocket/commits/a6f6390ba4f42d9c5b45f534cca4564a88b404fc))
* add JSON inventory fields to FieldRegistry with mixed-type support ([f6abe65](https://github.com/yourusername/SkyrimWebSocket/commits/f6abe65c1399c3a939e30f423983d4e154d371dc))
* add Player::Level, Player::XP::*, Player::InventoryWeight, Player::CarryWeight fields ([16a205a](https://github.com/yourusername/SkyrimWebSocket/commits/16a205a5c432234ed20846aa4d1c9ac1726ccec6))
* **ammo:** add new fields ([12db9e1](https://github.com/yourusername/SkyrimWebSocket/commits/12db9e1ed0f6c6c79beb21fcc70dc95d5c8f7d6e))
* enrich inventory data — enchantment details, body slots, damage formula, food, books, soul gem fill state, effect descriptions ([5e3e91e](https://github.com/yourusername/SkyrimWebSocket/commits/5e3e91e92844d4381618e24a962630759a2d1f96))
* expand inventory item data (equipped, favorite, descriptions, enchantments, etc.) ([50aeb34](https://github.com/yourusername/SkyrimWebSocket/commits/50aeb3432e908952127dd38a2434a01462e89014))
* inventory command system (equip/unequip/use/drop/favorite) ([#18](https://github.com/yourusername/SkyrimWebSocket/issues/18)) ([31873fc](https://github.com/yourusername/SkyrimWebSocket/commits/31873fc4212b24ec4b71fb0f95e39f7cb9e7a971))
* **inventory:** add categoryType field ([1fd492b](https://github.com/yourusername/SkyrimWebSocket/commits/1fd492b3a72e6dd8f1f0f0d6684197c45cd6419f))
* localized templates, isStolen, armorTypeId/armorType, categoryId, no hardcoded strings ([a989c21](https://github.com/yourusername/SkyrimWebSocket/commits/a989c2190b60dc671e52950b287f4b839f8d64ba))
* **magic:** add spells to favorite ([c85baa8](https://github.com/yourusername/SkyrimWebSocket/commits/c85baa8561cb5d3f859090f2443ca4afb56621a6))
* **magic:** equip spell command ([ca331a9](https://github.com/yourusername/SkyrimWebSocket/commits/ca331a9f4d375b5a5cdecf10f2ed1ce8a5eb999e))
* **maigc:** magic categories ([3185ec4](https://github.com/yourusername/SkyrimWebSocket/commits/3185ec4433a18235b1185086050a8ac841110609))
* remove describe type, add heartbeat message type ([6cbdc2c](https://github.com/yourusername/SkyrimWebSocket/commits/6cbdc2c5929729a890249b8968c154df545757cf))
* require id in query; add unsubscribe_all ([eaaa9ff](https://github.com/yourusername/SkyrimWebSocket/commits/eaaa9ff3971ba65740da6f315d189bd25bd38e24))
* support multiple named subscriptions per connection ([7030a3a](https://github.com/yourusername/SkyrimWebSocket/commits/7030a3ae5780bde267b9f8cfed49cafcd7b67182))


### 🐛 Bug Fixes

* access PlayerCharacter skills via GetInfoRuntimeData() for CommonLibSSE-NG multi-targeting ([e690709](https://github.com/yourusername/SkyrimWebSocket/commits/e6907097b4d03a28b5cbe027e2839394d1881823))
* address code review feedback in InventoryReader ([b909a39](https://github.com/yourusername/SkyrimWebSocket/commits/b909a397e641115bd4d4ca5fcc86f56e8acf2f62))
* create ExtraDataList for items without one when favoriting ([45abb76](https://github.com/yourusername/SkyrimWebSocket/commits/45abb762c77125fb42d8e123ac2762ae05d39dd0))
* **inventory:** build bugfix ([b5b2e5d](https://github.com/yourusername/SkyrimWebSocket/commits/b5b2e5d26c0cbcee2832660d5bed1f89234e821b))
* **inventory:** build fix ([339031f](https://github.com/yourusername/SkyrimWebSocket/commits/339031f5d19d1414775ece5730cbc9502acc3eaf))
* **magic:** add debug to find reason of unequip not working ([04bba9f](https://github.com/yourusername/SkyrimWebSocket/commits/04bba9f88a7fde97d232d561ce3639a11daf46c0))
* **magic:** check for equiped hand ([de71ab7](https://github.com/yourusername/SkyrimWebSocket/commits/de71ab72d279b844f8e90db4abf186e06c074e1b))
* **magic:** deselect spell ([b044dbd](https://github.com/yourusername/SkyrimWebSocket/commits/b044dbd0699c37a37f42ecb87ec6695c4f1ab4c5))
* **magic:** deselect spell for both hands bug ([c0715eb](https://github.com/yourusername/SkyrimWebSocket/commits/c0715eb340a517fc195aa6c3cc073f1988e6a0ff))
* **magic:** fix player known spell by equip command ([babc4b0](https://github.com/yourusername/SkyrimWebSocket/commits/babc4b0f5739635b53212147ce94df55fb6ec457))
* **magic:** level, right cost, unequip ([062bfdb](https://github.com/yourusername/SkyrimWebSocket/commits/062bfdbb1be706c1bcb4aeac41ca08e1b896f16b))
* **magic:** more spells of school reading ([c4bc962](https://github.com/yourusername/SkyrimWebSocket/commits/c4bc9629a7d4941c7513f875ace43081966e95e4))
* **magic:** wrong function call ([d20a8bd](https://github.com/yourusername/SkyrimWebSocket/commits/d20a8bd6c0b63128a940cb2e8d0768ab97f1ce23))
* **magix:** build error ([3e5bb03](https://github.com/yourusername/SkyrimWebSocket/commits/3e5bb0375a07cf04af9d80cbd6545faf0ccdb444))
* **maigc:** unequip master spell ([100fbd6](https://github.com/yourusername/SkyrimWebSocket/commits/100fbd6560ea5c0b6f96a1f7882b805397eaae72))
* read descriptionTemplate from magicItemDescription (DNAM), not TESDescription ([cb6315c](https://github.com/yourusername/SkyrimWebSocket/commits/cb6315c4e0b22617d5514c76c2502e8e23cba8eb))
* remove const on TESDescription/TESObjectARMO pointers to fix C2662 build errors ([8cb4f06](https://github.com/yourusername/SkyrimWebSocket/commits/8cb4f061ee6569d6d800e332cfb0cda8407fd3a6))
* remove misleading GMST mappings for categories; fix PROTOCOL.md example ([63e1990](https://github.com/yourusername/SkyrimWebSocket/commits/63e1990348c0c0f592dc417dce02c0d1b527ed98))
* replace non-existent ExtraStolenFlag with ExtraOwnership check ([fab0d61](https://github.com/yourusername/SkyrimWebSocket/commits/fab0d61379e8545d64ef995519ca75797cfa4f04))
* **semantic:** corerct change release version ([6e29c29](https://github.com/yourusername/SkyrimWebSocket/commits/6e29c29899874d3d30959b8c28d2c3f8390ceb57))
* static_cast for descriptionTemplate + add baseArmorRating field ([a76cdb9](https://github.com/yourusername/SkyrimWebSocket/commits/a76cdb901fda9d868aae816327a17c114713fba4))
* use correct RE::FormType enum values (no k prefix) for CommonLibSSE ([4040110](https://github.com/yourusername/SkyrimWebSocket/commits/40401107c808637e42e4a3d4fa99daaa3ff92552))
* use engine SetFavorite/RemoveFavorite instead of manual ExtraDataList construction ([5b3f73b](https://github.com/yourusername/SkyrimWebSocket/commits/5b3f73b055a6e04a9e3fa78f67cff77ae0ce2ce2))

### [1.0.1](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.0.1) (2026-04-07)


### ✨ Features

* add Base and Clamped ActorValue getter ([7ffbf0a](https://github.com/yourusername/SkyrimWebSocket/commits/7ffbf0af98c13c59d878cdd36241140b370da43c))
* enhance FieldRegistry to support current and permanent value types ([002f1ba](https://github.com/yourusername/SkyrimWebSocket/commits/002f1ba95142697d4c9291a8830d8428b463a8dc))


### 🐛 Bug Fixes

* restrict build workflow to main branch only ([e1a445f](https://github.com/yourusername/SkyrimWebSocket/commits/e1a445f30687b1090557af6018e8aacb8ddfe24a))

## [1.4.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.2.0...v1.4.0) (2026-04-14)


### ✨ Features

* add description field with <mag>/<dur> substituted in effect entries ([e74c24d](https://github.com/yourusername/SkyrimWebSocket/commits/e74c24d710bd29295ae744edcf25c587e2a01411))
* add Game::Language field to retrieve current game language ([a6f6390](https://github.com/yourusername/SkyrimWebSocket/commits/a6f6390ba4f42d9c5b45f534cca4564a88b404fc))
* add JSON inventory fields to FieldRegistry with mixed-type support ([f6abe65](https://github.com/yourusername/SkyrimWebSocket/commits/f6abe65c1399c3a939e30f423983d4e154d371dc))
* add Player::Level, Player::XP::*, Player::InventoryWeight, Player::CarryWeight fields ([16a205a](https://github.com/yourusername/SkyrimWebSocket/commits/16a205a5c432234ed20846aa4d1c9ac1726ccec6))
* enrich inventory data — enchantment details, body slots, damage formula, food, books, soul gem fill state, effect descriptions ([5e3e91e](https://github.com/yourusername/SkyrimWebSocket/commits/5e3e91e92844d4381618e24a962630759a2d1f96))
* expand inventory item data (equipped, favorite, descriptions, enchantments, etc.) ([50aeb34](https://github.com/yourusername/SkyrimWebSocket/commits/50aeb3432e908952127dd38a2434a01462e89014))
* inventory command system (equip/unequip/use/drop/favorite) ([#18](https://github.com/yourusername/SkyrimWebSocket/issues/18)) ([31873fc](https://github.com/yourusername/SkyrimWebSocket/commits/31873fc4212b24ec4b71fb0f95e39f7cb9e7a971))
* localized templates, isStolen, armorTypeId/armorType, categoryId, no hardcoded strings ([a989c21](https://github.com/yourusername/SkyrimWebSocket/commits/a989c2190b60dc671e52950b287f4b839f8d64ba))
* remove describe type, add heartbeat message type ([6cbdc2c](https://github.com/yourusername/SkyrimWebSocket/commits/6cbdc2c5929729a890249b8968c154df545757cf))
* require id in query; add unsubscribe_all ([eaaa9ff](https://github.com/yourusername/SkyrimWebSocket/commits/eaaa9ff3971ba65740da6f315d189bd25bd38e24))
* support multiple named subscriptions per connection ([7030a3a](https://github.com/yourusername/SkyrimWebSocket/commits/7030a3ae5780bde267b9f8cfed49cafcd7b67182))


### 🐛 Bug Fixes

* access PlayerCharacter skills via GetInfoRuntimeData() for CommonLibSSE-NG multi-targeting ([e690709](https://github.com/yourusername/SkyrimWebSocket/commits/e6907097b4d03a28b5cbe027e2839394d1881823))
* address code review feedback in InventoryReader ([b909a39](https://github.com/yourusername/SkyrimWebSocket/commits/b909a397e641115bd4d4ca5fcc86f56e8acf2f62))
* create ExtraDataList for items without one when favoriting ([45abb76](https://github.com/yourusername/SkyrimWebSocket/commits/45abb762c77125fb42d8e123ac2762ae05d39dd0))
* read descriptionTemplate from magicItemDescription (DNAM), not TESDescription ([cb6315c](https://github.com/yourusername/SkyrimWebSocket/commits/cb6315c4e0b22617d5514c76c2502e8e23cba8eb))
* remove const on TESDescription/TESObjectARMO pointers to fix C2662 build errors ([8cb4f06](https://github.com/yourusername/SkyrimWebSocket/commits/8cb4f061ee6569d6d800e332cfb0cda8407fd3a6))
* remove misleading GMST mappings for categories; fix PROTOCOL.md example ([63e1990](https://github.com/yourusername/SkyrimWebSocket/commits/63e1990348c0c0f592dc417dce02c0d1b527ed98))
* replace non-existent ExtraStolenFlag with ExtraOwnership check ([fab0d61](https://github.com/yourusername/SkyrimWebSocket/commits/fab0d61379e8545d64ef995519ca75797cfa4f04))
* **semantic:** corerct change release version ([6e29c29](https://github.com/yourusername/SkyrimWebSocket/commits/6e29c29899874d3d30959b8c28d2c3f8390ceb57))
* static_cast for descriptionTemplate + add baseArmorRating field ([a76cdb9](https://github.com/yourusername/SkyrimWebSocket/commits/a76cdb901fda9d868aae816327a17c114713fba4))
* use correct RE::FormType enum values (no k prefix) for CommonLibSSE ([4040110](https://github.com/yourusername/SkyrimWebSocket/commits/40401107c808637e42e4a3d4fa99daaa3ff92552))
* use engine SetFavorite/RemoveFavorite instead of manual ExtraDataList construction ([5b3f73b](https://github.com/yourusername/SkyrimWebSocket/commits/5b3f73b055a6e04a9e3fa78f67cff77ae0ce2ce2))

### [1.0.1](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.0.1) (2026-04-07)


### ✨ Features

* add Base and Clamped ActorValue getter ([7ffbf0a](https://github.com/yourusername/SkyrimWebSocket/commits/7ffbf0af98c13c59d878cdd36241140b370da43c))
* enhance FieldRegistry to support current and permanent value types ([002f1ba](https://github.com/yourusername/SkyrimWebSocket/commits/002f1ba95142697d4c9291a8830d8428b463a8dc))


### 🐛 Bug Fixes

* restrict build workflow to main branch only ([e1a445f](https://github.com/yourusername/SkyrimWebSocket/commits/e1a445f30687b1090557af6018e8aacb8ddfe24a))

## [1.3.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.2.0...v1.3.0) (2026-04-09)


### 🐛 Bug Fixes

* address code review feedback in InventoryReader ([b909a39](https://github.com/yourusername/SkyrimWebSocket/commits/b909a397e641115bd4d4ca5fcc86f56e8acf2f62))
* read descriptionTemplate from magicItemDescription (DNAM), not TESDescription ([cb6315c](https://github.com/yourusername/SkyrimWebSocket/commits/cb6315c4e0b22617d5514c76c2502e8e23cba8eb))
* remove const on TESDescription/TESObjectARMO pointers to fix C2662 build errors ([8cb4f06](https://github.com/yourusername/SkyrimWebSocket/commits/8cb4f061ee6569d6d800e332cfb0cda8407fd3a6))
* remove misleading GMST mappings for categories; fix PROTOCOL.md example ([63e1990](https://github.com/yourusername/SkyrimWebSocket/commits/63e1990348c0c0f592dc417dce02c0d1b527ed98))
* replace non-existent ExtraStolenFlag with ExtraOwnership check ([fab0d61](https://github.com/yourusername/SkyrimWebSocket/commits/fab0d61379e8545d64ef995519ca75797cfa4f04))
* **semantic:** corerct change release version ([6e29c29](https://github.com/yourusername/SkyrimWebSocket/commits/6e29c29899874d3d30959b8c28d2c3f8390ceb57))
* static_cast for descriptionTemplate + add baseArmorRating field ([a76cdb9](https://github.com/yourusername/SkyrimWebSocket/commits/a76cdb901fda9d868aae816327a17c114713fba4))
* use correct RE::FormType enum values (no k prefix) for CommonLibSSE ([4040110](https://github.com/yourusername/SkyrimWebSocket/commits/40401107c808637e42e4a3d4fa99daaa3ff92552))


### ✨ Features

* add description field with <mag>/<dur> substituted in effect entries ([e74c24d](https://github.com/yourusername/SkyrimWebSocket/commits/e74c24d710bd29295ae744edcf25c587e2a01411))
* add JSON inventory fields to FieldRegistry with mixed-type support ([f6abe65](https://github.com/yourusername/SkyrimWebSocket/commits/f6abe65c1399c3a939e30f423983d4e154d371dc))
* enrich inventory data — enchantment details, body slots, damage formula, food, books, soul gem fill state, effect descriptions ([5e3e91e](https://github.com/yourusername/SkyrimWebSocket/commits/5e3e91e92844d4381618e24a962630759a2d1f96))
* expand inventory item data (equipped, favorite, descriptions, enchantments, etc.) ([50aeb34](https://github.com/yourusername/SkyrimWebSocket/commits/50aeb3432e908952127dd38a2434a01462e89014))
* localized templates, isStolen, armorTypeId/armorType, categoryId, no hardcoded strings ([a989c21](https://github.com/yourusername/SkyrimWebSocket/commits/a989c2190b60dc671e52950b287f4b839f8d64ba))

### [1.0.1](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.0.1) (2026-04-07)


### ✨ Features

* add Base and Clamped ActorValue getter ([7ffbf0a](https://github.com/yourusername/SkyrimWebSocket/commits/7ffbf0af98c13c59d878cdd36241140b370da43c))
* enhance FieldRegistry to support current and permanent value types ([002f1ba](https://github.com/yourusername/SkyrimWebSocket/commits/002f1ba95142697d4c9291a8830d8428b463a8dc))


### 🐛 Bug Fixes

* restrict build workflow to main branch only ([e1a445f](https://github.com/yourusername/SkyrimWebSocket/commits/e1a445f30687b1090557af6018e8aacb8ddfe24a))

### [1.2.1](https://github.com/yourusername/SkyrimWebSocket/compare/v1.2.0...v1.2.1) (2026-04-08)


### 🐛 Bug Fixes

* **semantic:** corerct change release version ([6e29c29](https://github.com/yourusername/SkyrimWebSocket/commits/6e29c29899874d3d30959b8c28d2c3f8390ceb57))

## [1.2.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.2.0) (2026-04-07)


### ✨ Features

* **ci:** add semantic versioning support ([e9a9a3c](https://github.com/yourusername/SkyrimWebSocket/commits/e9a9a3cd73dd467c6f5ab81ed122c1f9edbcddf8))


### 🐛 Bug Fixes

* **ci:** update commit prehooks ([0088851](https://github.com/yourusername/SkyrimWebSocket/commits/008885106430fc5c73f5df08cea5756ff9c84aa8))

## [1.1.0](https://github.com/yourusername/SkyrimWebSocket/compare/v1.0.0...v1.1.0) (2026-04-07)


### ✨ Features

* **ci:** add semantic versioning support ([e9a9a3c](https://github.com/yourusername/SkyrimWebSocket/commits/e9a9a3cd73dd467c6f5ab81ed122c1f9edbcddf8))


### 🐛 Bug Fixes

* **ci:** update commit prehooks ([0088851](https://github.com/yourusername/SkyrimWebSocket/commits/008885106430fc5c73f5df08cea5756ff9c84aa8))

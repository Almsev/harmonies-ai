/**
 * Harminies Game - JavaScript Wrapper
 * Provides a clean JavaScript API over the WASM module
 */

class HarminiesGame {
  constructor(wasmModule) {
    this.module = wasmModule;
    
    // Wrap C functions
    this.wasm_init_game = this.module.cwrap('wasm_init_game', null, ['number', 'number', 'number', 'number']);
    this.wasm_get_actions = this.module.cwrap('wasm_get_actions', 'number', ['number']);
    this.wasm_do_action = this.module.cwrap('wasm_do_action', 'number', ['number']);
    this.wasm_player_score = this.module.cwrap('wasm_player_score', 'number', ['number']);
    this.wasm_get_ai_action = this.module.cwrap('wasm_get_ai_action', 'number', []);
    this.wasm_get_state = this.module.cwrap('wasm_get_state', 'number', []);
    this.wasm_get_animal = this.module.cwrap('wasm_get_animal', 'number', ['number']);
    this.wasm_get_animal_count = this.module.cwrap('wasm_get_animal_count', 'number', []);
    this.wasm_get_spirit_animal_start = this.module.cwrap('wasm_get_spirit_animal_start', 'number', []);
    this.wasm_get_spirit_animal_count = this.module.cwrap('wasm_get_spirit_animal_count', 'number', []);
    this.wasm_set_ai_speed = this.module.cwrap('wasm_set_ai_speed', null, ['number']);
    this.wasm_set_use_dlc = this.module.cwrap('wasm_set_use_dlc', null, ['number']);
    
    // Constants from C code
    this.MAX_ACTIONS = 256;
    this.BOARD_SIZE = 25;
    this.MAX_PLAYERS = 4;
    this.MAX_ANIMALS_PER_PLAYER = 4;
    this.ANIMAL_DECK_CAPACITY = 42;
    this.spiritAnimalStart = this.wasm_get_spirit_animal_start();
    this.spiritAnimalCount = this.wasm_get_spirit_animal_count();
    this.totalAnimalCount = this.wasm_get_animal_count();
    
    // Action types
    this.ACTION_TYPES = {
      CHOOSE_TOKENS: 0x01,
      PLACE_TOKEN: 0x02,
      CHOOSE_ANIMAL: 0x03,
      PLACE_ANIMAL: 0x04,
      END_ROUND: 0x05,
      CHOOSE_SPIRIT: 0x06
    };

    // Internal state
    this.playerCount = 2;
    this.boardVariant = 0; // 0 = river, 1 = islands
    this.useSpirits = 1; // 0 = no spirits, 1 = spirits, 2 = spirits + T-Rex promo
    this.useDlc = 0; // 0 = base game only, 1 = include Pulse DLC animals
    this.allBotMode = 0; // 0 = player 0 human, 1 = all players are bots
    this.currentSeed = 0;
    this.actionLog = []; // Array of {player, action}
    this.gameFinished = false;
    this.storageKey = 'harminies_game_state';
    this.aiSpeed = 2; // 0 = Quick, 1 = Normal, 2 = Slow, 3 = Ultra Slow
  }
  
  /**
   * Initialize a new game
   * @param {number} seed - Random seed for the game
   * @param {number} playerCount - Number of players (2-4)
   * @param {number} boardVariant - Board variant (0 = river, 1 = islands)
  * @param {number} useSpirits - Use spirits mode (0 = no, 1 = yes, 2 = yes + T-Rex)
   * @param {number} useDlc - Include Pulse DLC normal animals (0 = no, 1 = yes)
    * @param {number} aiSpeed - Bot thinking speed (0 = Quick, 1 = Normal, 2 = Slow, 3 = Ultra Slow)
   * @param {number} allBotMode - Enable full bot autoplay (0 = no, 1 = yes)
   */
  initGame(seed = Date.now(), playerCount = 2, boardVariant = 0, useSpirits = 1, useDlc = 0, aiSpeed = 2, allBotMode = 0) {
    this.playerCount = playerCount;
    this.boardVariant = boardVariant;
    this.useSpirits = useSpirits;
    this.useDlc = useDlc;
    this.aiSpeed = aiSpeed;
    this.allBotMode = allBotMode;
    this.currentSeed = seed >>> 0; // Ensure unsigned 32-bit seed
    this.actionLog = []; // Clear action log
    this.gameFinished = false;
    this.wasm_set_ai_speed(aiSpeed);
    this.wasm_set_use_dlc(useDlc);
    this.wasm_init_game(this.currentSeed, playerCount, boardVariant, useSpirits);
    this.saveToStorage();
  }
  
  /**
   * Save game state to localStorage
   */
  saveToStorage() {
    try {
      const state = {
        seed: this.currentSeed,
        playerCount: this.playerCount,
        boardVariant: this.boardVariant,
        useSpirits: this.useSpirits,
        useDlc: this.useDlc,
        allBotMode: this.allBotMode,
        actions: this.actionLog.map(entry => entry.action),
      };
      state.aiSpeed = this.aiSpeed; // Save AI speed to state
      localStorage.setItem(this.storageKey, JSON.stringify(state));
    } catch (error) {
      console.warn('Failed to save game state to localStorage:', error);
    }
  }
  
  /**
   * Load game state from localStorage and restore it
   * @returns {boolean} True if state was restored, false otherwise
   */
  loadFromStorage() {
    try {
      const savedState = localStorage.getItem(this.storageKey);
      if (!savedState) {
        return false;
      }
      
      const state = JSON.parse(savedState);
      
      // Default values for backward compatibility
      const boardVariant = state.boardVariant !== undefined ? state.boardVariant : 0;
      const useSpirits = state.useSpirits !== undefined ? state.useSpirits : 1;
      const useDlc = state.useDlc !== undefined ? state.useDlc : 0;
      const aiSpeed = state.aiSpeed !== undefined ? state.aiSpeed : 2;
      const allBotMode = state.allBotMode !== undefined ? state.allBotMode : 0;
      
      // Reinitialize with saved parameters
      this.wasm_set_ai_speed(aiSpeed);
      this.wasm_set_use_dlc(useDlc);
      this.wasm_init_game(state.seed, state.playerCount, boardVariant, useSpirits);
      this.currentSeed = state.seed;
      this.playerCount = state.playerCount;
      this.boardVariant = boardVariant;
      this.useSpirits = useSpirits;
      this.useDlc = useDlc;
      this.aiSpeed = aiSpeed;
      this.allBotMode = allBotMode;
      this.actionLog = [];
      this.gameFinished = false;
      
      // Replay all saved actions
      for (const action of state.actions) {
        const currentPlayer = this.getCurrentPlayer();
        const animalIndex = this.getAnimalIndexFromAction(action);
        const result = this.wasm_do_action(action);
        this.actionLog.push({
          player: currentPlayer,
          action: action,
          animalIndex
        });
        if (result === 1) {
          this.gameFinished = true;
          break;
        }
      }
      
      console.log(`Restored game state: ${state.actions.length} actions replayed`);
      return true;
    } catch (error) {
      console.warn('Failed to load game state from localStorage:', error);
      return false;
    }
  }
  
  /**
   * Clear saved game state from localStorage
   */
  clearStorage() {
    try {
      localStorage.removeItem(this.storageKey);
    } catch (error) {
      console.warn('Failed to clear game state from localStorage:', error);
    }
  }
  
  /**
   * Get all possible actions for the current state
   * @returns {number[]} Array of action codes
   */
  getActions() {
    if (this.gameFinished) return [];
    const actionsPtr = this.module._malloc(this.MAX_ACTIONS * 2); // uint16_t
    try {
      const count = this.wasm_get_actions(actionsPtr);
      const actions = [];
      for (let i = 0; i < count; i++) {
        actions.push(this.module.getValue(actionsPtr + i * 2, 'i16'));
      }
      return actions;
    } finally {
      this.module._free(actionsPtr);
    }
  }
  
  /**
   * Decode an action code into human-readable format
   * @param {number} action - Action code
   * @returns {Object} Decoded action
   */
  decodeAction(action) {
    const type = (action >> 8) & 0xFF;
    const params = action & 0xFF;
    
    switch (type) {
      case this.ACTION_TYPES.CHOOSE_TOKENS:
        return {
          type: 'CHOOSE_TOKENS',
          supplyIndex: params & 0x7
        };
      
      case this.ACTION_TYPES.PLACE_TOKEN:
        return {
          type: 'PLACE_TOKEN',
          tokenIndex: (params >> 5) & 0x7,
          boardPosition: params & 0x1F
        };
      
      case this.ACTION_TYPES.CHOOSE_ANIMAL:
        return {
          type: 'CHOOSE_ANIMAL',
          animalIndex: params & 0x7
        };
      
      case this.ACTION_TYPES.PLACE_ANIMAL:
        return {
          type: 'PLACE_ANIMAL',
          animalSlot: (params >> 5) & 0x3,
          boardPosition: params & 0x1F
        };
      
      case this.ACTION_TYPES.END_ROUND:
        return {
          type: 'END_ROUND'
        };
      
      case this.ACTION_TYPES.CHOOSE_SPIRIT:
        return {
          type: 'CHOOSE_SPIRIT',
          spiritIndex: params & 0x7
        };
      
      default:
        return {
          type: 'UNKNOWN',
          raw: action
        };
    }
  }
  
  /**
   * Execute an action
   * @param {number} action - Action code
   * @returns {number} 1 if game ended, 0 otherwise
   */
  doAction(action) {
    if (this.gameFinished) {
      console.warn('Game is already finished. Cannot perform more actions.');
      return 1;
    }
    
    const currentPlayer = this.getCurrentPlayer();
    const animalIndex = this.getAnimalIndexFromAction(action);
    const result = this.wasm_do_action(action);
    
    // Log the action with the player who made it
    this.actionLog.push({
      player: currentPlayer,
      action: action,
      animalIndex
    });
    
    // Update game finished state
    if (result === 1) {
      this.gameFinished = true;
    }
    
    // Save to storage after each action
    this.saveToStorage();
    
    return result;
  }
  
  /**
   * Undo the last action by replaying the game from the beginning
   * @returns {boolean} True if undo was successful, false if no actions to undo
   */
  undo() {
    if (this.actionLog.length === 0) {
      console.warn('No actions to undo');
      return false;
    }
    
    // Remove the last action
    const actionsToReplay = this.actionLog.slice(0, -1);
    
    // Reinitialize the game with the same seed and player count
    this.wasm_set_ai_speed(this.aiSpeed);
    this.wasm_set_use_dlc(this.useDlc);
    this.wasm_init_game(this.currentSeed, this.playerCount, this.boardVariant, this.useSpirits);
    this.actionLog = [];
    this.gameFinished = false;
    
    // Replay all actions except the last one
    for (const entry of actionsToReplay) {
      const result = this.wasm_do_action(entry.action);
      this.actionLog.push(entry);
      if (result === 1) {
        this.gameFinished = true;
        break;
      }
    }
    
    // Save updated state to storage
    this.saveToStorage();
    
    return true;
  }
  canUndo() {
    if (this.actionLog.length === 0) return false;
    const lastEntry = this.actionLog[this.actionLog.length - 1];
    const currentPlayer = this.getCurrentPlayer();
    return lastEntry.player === currentPlayer;
  }
  
  /**
   * Get the action log
   * @returns {Array} Array of {player, action} objects
   */
  getActionLog() {
    return [...this.actionLog]; // Return a copy
  }
  
  /**
   * Check if the game is finished
   * @returns {boolean} True if game is finished
   */
  isGameFinished() {
    return this.gameFinished;
  }
  
  /**
   * Get AI's chosen action
   * @returns {number} Action code
   */
  getAIAction() {
    return this.wasm_get_ai_action();
  }
  
  /**
   * Get player's score
   * @param {number} playerIndex - Player index (0-3)
   * @returns {number} Score
   */
  getPlayerScore(playerIndex) {
    const componentsPtr = this.module._malloc(8); // uint8_t[8]
    const score = this.wasm_player_score(playerIndex, componentsPtr);
    const components = new Uint8Array(this.module.HEAP8.buffer, componentsPtr, 8);
    this.module._free(componentsPtr);
    return { score,
      trees: components[0],
      mountains: components[1],
      fields: components[2],
      buildings: components[3],
      water: components[4],
      animals: components[5],
      spirit: components[6],
      tiebreak: components[7]
     };
  }
  
  /**
   * Get the full game state as a JavaScript object
   * @returns {Object} Game state
   */
  getState() {
    const statePtr = this.wasm_get_state();
    
    // Use the exported HEAP arrays directly
    const HEAP8 = this.module.HEAP8;
    const HEAP16 = this.module.HEAP16;
    const HEAP32 = this.module.HEAP32;
    
    let offset = statePtr;
    
    // Read state structure (must match C struct layout)
    const state = {
      rngState: HEAP32[offset >> 2],
      spiritDeck: HEAP16[(offset + 4) >> 1],
      round: HEAP8[offset + 6],
      roundState: HEAP8[offset + 7],
      animalDeckSize: HEAP8[offset + 8],
      animalDeck: [],
      tokenBag: [],
      animals: [],
      tokenSupply: [],
      players: []
    };
    
    offset += 9;
    
    // Animal deck (42 bytes with DLC capacity)
    for (let i = 0; i < this.ANIMAL_DECK_CAPACITY; i++) {
      state.animalDeck.push(HEAP8[offset++]);
    }
    
    // Token bag (6 bytes)
    for (let i = 0; i < 6; i++) {
      state.tokenBag.push(HEAP8[offset++]);
    }
    
    // Animals (5 int8_t)
    for (let i = 0; i < 5; i++) {
      const val = HEAP8[offset++];
      state.animals.push(val === -1 ? null : val);
    }
    
    // Token supply (5x3 int8_t)
    for (let i = 0; i < 5; i++) {
      const supply = [];
      for (let j = 0; j < 3; j++) {
        const val = HEAP8[offset++];
        supply.push(val === -1 ? null : val);
      }
      state.tokenSupply.push(supply);
    }

    // Align to next boundary if needed
    while (offset % 2 !== 0) offset++;
    
    // Players (4 players)
    for (let p = 0; p < this.MAX_PLAYERS; p++) {
      const player = {
        board: [],
        animals: [],
        score: 0
      };
      
      // Board (25 bytes)
      for (let i = 0; i < this.BOARD_SIZE; i++) {
        player.board.push(HEAP8[offset++]);
      }
      
      // Align to 16-bit boundary
      if (offset % 2 !== 0) offset++;
      
      // Animals (4 int16_t)
      for (let i = 0; i < this.MAX_ANIMALS_PER_PLAYER; i++) {
        const val = HEAP16[offset >> 1];
        if (val === -1) {
          player.animals.push(null);
        } else {
          player.animals.push({
            id: val >> 3,
            progress: val & 0x7
          });
        }
        offset += 2;
      }
      
      // Score (uint8_t)
      player.score = HEAP8[offset++];

      player.spiritState = HEAP8[offset++];
      
      // Padding to next player
      while (offset % 2 !== 0) offset++;
      
      if (p < this.playerCount) state.players.push(player);
    }
    
    return state;
  }
  
  
  /**
   * Get current player index
   * @returns {number} Current player (0-3)
   */
  getCurrentPlayer() {
    const state = this.getState();
    return state.round % this.playerCount;
  }
  getAnimalIndex(supplyIndex) {
    const state = this.getState();
    const supply = state.animals[supplyIndex];
    return supply;
  }
  getPlayerAnimalIndex(index) {
    const state = this.getState();
    return state.players[state.round % this.playerCount].animals[index].id;
  }
  getAnimalIndexFromAction(action) {
    const type = (action >> 8) & 0xFF;
    return type === this.ACTION_TYPES.CHOOSE_ANIMAL ? this.getAnimalIndex(action & 7) :
    type === this.ACTION_TYPES.CHOOSE_SPIRIT ? this.getPlayerAnimalIndex(action & 1) : null;
  }
  getPlayerAnimals(playerIndex) {
    return this.actionLog.filter(({player, animalIndex}) => player === playerIndex && animalIndex !== null).map(({animalIndex}) => animalIndex);
  }
    
  /**
   * Get an animal struct from WASM and convert to JS object
   * @param {number} index - Animal index in the deck
  * @returns {Object|null} { token, env: [{token,positions},...], score: [..], hasAlternativeComposition } or null if pointer is 0
   */
  getAnimal(index) {
    const ptr = this.wasm_get_animal(index);
    if (!ptr) return null;

    const HEAP8 = this.module.HEAP8;
    const HEAPU8 = this.module.HEAPU8;
    let off = ptr;

    // token (uint8_t)
    const token = HEAP8[off++];

    // env[3] each { uint8_t token; uint8_t pos_a; uint8_t pos_b; }
    const env = [];
    let hasAlternativeComposition = false;
    for (let i = 0; i < 3; i++) {
      const envToken = HEAP8[off++];
      const envPosA = HEAPU8[off++];
      const envPosB = HEAPU8[off++];
      const tokenPosMap = {
        0x32: 0,
        0x33: 0,
        0xF3: 1,
        0xF0: 3,
        0x34: 4,
        0xF4: 5,
        0xF5: 6,
        0xF1: 2,
        0xF2: 4,
      }
      if (envToken) {
        const posA = tokenPosMap[envPosA] !== undefined ? tokenPosMap[envPosA] : 1;
        const mappedPosB = tokenPosMap[envPosB];
        const posB = mappedPosB !== undefined ? mappedPosB : posA;
        if (posB !== posA) hasAlternativeComposition = true;
        env.push({
          token: envToken,
          positions: [posA, posB],
          pos: posA,
          altPos: posB,
        });
      }
    }

    // score[MAX_ANIMAL_PROGRESS] (MAX_ANIMAL_PROGRESS == 5)
    const score = [];
    const scoreType = HEAP8[off];
    let total = 0;
    if (scoreType === -1) {
      const scoresPerSize = [HEAP8[off+1], HEAP8[off+2], HEAP8[off+3]];
      if (scoresPerSize[2] === scoresPerSize[1]) {
        scoresPerSize.pop();
        if (scoresPerSize[1] === scoresPerSize[0]) scoresPerSize.pop();
      }
      const mappedScores = scoresPerSize.map((s, i) => ({s, i: i+1})).filter(({s}) => s);
      if (mappedScores.length > 1 && mappedScores[0].s === mappedScores[1].s) {
        mappedScores.shift();
        mappedScores[0].i = '1-2';
      }
      mappedScores[mappedScores.length-1].i += '+';
      score.push('$'+token);
      score.push(mappedScores.map(({s, i}) =>
        `${i}: ${s}`
      ).join(', '));
    } else if (scoreType === -2) {
      const scoresPerHeight = [HEAP8[off+1], HEAP8[off+2], HEAP8[off+3]];
      const color = this.tokenToColor[token];
      this.tokenToColor.map((c, i) => ({ token: i, color: c, height: this.tokenToHeight[i] })).filter(t => t.color === color).forEach(({token, height}) => {
        if (scoresPerHeight[height-1]) score.push('$' + token, `${scoresPerHeight[height-1]}`);
      });
    } else for (let i = 0; i < 5; i++) {
      const s = HEAP8[off++];
      total += s;
      if (s) score.push(total);
    }

    return { token, env, score, hasAlternativeComposition };
  }

  getAnimalCount() {
    return this.totalAnimalCount;
  }

  isSpiritAnimal(animalId) {
    return animalId >= this.spiritAnimalStart && animalId < this.spiritAnimalStart + this.spiritAnimalCount;
  }
    
  /**
   * Convert board index to x,y coordinates
   * @param {number} index - Board index
   * @param {number} width - Board width
   * @returns {Object} {x, y} coordinates
   */
  indexToXY(index, width = 5) {
    const evenCount = Math.floor((width + 1) / 2);
    const row = Math.floor(index / width) * 2;
    const col = index % width;
    
    if (col >= evenCount) {
      return { x: col - evenCount, y: row + 1 };
    } else {
      return { x: col, y: row };
    }
  }
  
  /**
   * Convert x,y coordinates to board index
   * @param {number} x - X coordinate
   * @param {number} y - Y coordinate
   * @param {number} width - Board width
   * @returns {number} Board index
   */
  xyToIndex(x, y, width = 5) {
    const odd = y & 1;
    const evenCount = Math.floor((width + 1) / 2);
    const oddCount = Math.floor(width / 2);
    
    if (x < 0 || x >= (odd ? oddCount : evenCount)) return -1;
    
    return Math.floor(y / 2) * width + (odd ? evenCount : 0) + x;
  }
  
  /**
   * Format state for display
   * @returns {string} Formatted state
   */
  formatState() {
    const state = this.getState();
    let output = `Round: ${state.round}\n`;
    output += `Current Player: ${this.getCurrentPlayer()}\n\n`;
    
    output += `Animals Available:\n`;
    state.animals.forEach((animal, i) => {
      output += `  [${i}] ${animal !== null ? `Animal #${animal}` : 'Empty'}\n`;
    });
    
    output += `\nToken Supply:\n`;
    state.tokenSupply.forEach((supply, i) => {
      const tokens = supply.map(t => t !== null ? this.getTokenName(t) : '--').join(' ');
      output += `  [${i}] ${tokens}\n`;
    });
    
    output += `\nPlayers:\n`;
    state.players.forEach((player, i) => {
      output += `  Player ${i}: Score ${player.score}\n`;
    });
    
    return output;
  }
  tokenToColor = [ 0, 1, 2, 3, 4, 5, 6, 6, 4, 4, 5, 5, 3 ];
  tokenToHeight = [ 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 2, 3, 2 ];
}

// Export for use in browser or Node.js
if (typeof module !== 'undefined' && module.exports) {
  module.exports = HarminiesGame;
}

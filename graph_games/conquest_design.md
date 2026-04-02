# CONQUEST - Game Design Document

## Overview

A Risk-style territory conquest game using real world geography. Players take
turns deploying armies, attacking adjacent territories with dice, and working
toward world domination.

- **Name:** Conquest
- **Subtitle:** None required - clean standalone title

### Legal Notes

- Core mechanics (dice combat, territory control, card reinforcement,
  elimination) are not protectable
- Real world geography is not protectable
- Original artwork and map required - do not copy Hasbro's specific visual style
- Do not use the name "Risk" except in descriptive context

## Territories (44 total)

### NORTH AMERICA (+5 bonus) - 10 territories

1. Alaska
2. Western Canada
3. Eastern Canada
4. Pacific Coast (US)
5. Mountain West (US)
6. Central US
7. Southern US
8. Northeast US
9. Mexico
10. Central America

### SOUTH AMERICA (+2 bonus) - 5 territories

11. Colombia & Venezuela
12. Brazil North
13. Brazil South
14. Peru & Bolivia
15. Argentina & Chile

### EUROPE (+5 bonus) - 7 territories

16. Britain & Ireland
17. Scandinavia
18. Western Europe
19. Central Europe
20. Southern Europe
21. Eastern Europe
22. Ukraine & Balkans

### AFRICA (+3 bonus) - 6 territories

23. North Africa
24. West Africa
25. East Africa
26. Central Africa
27. Southern Africa
28. Madagascar

### MIDDLE EAST & CENTRAL ASIA (+3 bonus) - 4 territories

29. Turkey & Caucasus
30. Middle East
31. Arabian Peninsula
32. Central Asia

### ASIA (+7 bonus) - 8 territories

33. Russia West
34. Russia Central
35. Russia East (Siberia)
36. China North
37. China South
38. India
39. Southeast Asia
40. Korea & Japan

### OCEANIA (+2 bonus) - 4 territories

41. Western Australia
42. Eastern Australia
43. New Zealand
44. Indonesia & Papua

## Key Chokepoints

- Alaska <-> Russia East (Siberia) - Bering Strait connection
- Central America - sole land bridge between North and South America
- Turkey & Caucasus / Middle East - bridge between Europe, Asia, and Africa
- Indonesia & Papua - connection between Asia and Oceania
- North Africa - connection between Africa and Europe/Middle East

## Continent Bonus Summary

| Continent               | Bonus |
|--------------------------|-------|
| North America            | +5    |
| South America            | +2    |
| Europe                   | +5    |
| Africa                   | +3    |
| Middle East & Central Asia | +3  |
| Asia                     | +7    |
| Oceania                  | +2    |
| **Total available**      | **+27** |

## Core Mechanics

### Combat

- Attacker rolls up to 3 dice, defender rolls up to 2 dice
- Compare highest dice, then second highest if applicable
- Higher roll wins each comparison; ties go to defender
- Army count on attacking vs defending territory influences odds

### Reinforcement

- Card set trading for reinforcements (sets increase in value each trade)
- Continent bonuses awarded when a player controls all territories in a
  continent
- Minimum reinforcement of 3 armies per turn

### Turn Phases

1. **Deploy** - Place reinforcement armies on owned territories
2. **Attack** - Attack adjacent enemy territories (optional, any number)
3. **Fortify** - Move armies between connected owned territories (one move)

### Victory

- Eliminate all other players to win
- A player is eliminated when they lose their last territory

## Game Modes

### Player Configuration

- 2 to 5 players
- Any combination of human and computer players
- Computer AI assigned to any player slots

### Menu Structure

- **Game**
  - New Game
  - Exit
- **Players** (configure before starting)
  - 2 Players / 3 Players / 4 Players / 5 Players
  - Toggle each player slot between Human and Computer
- **Help**
  - About Conquest
  - Rules

### Computer AI

- Evaluates territory control, continent progress, and army concentration
- Prioritizes completing continents
- Defends chokepoints
- Varies aggression based on relative strength

## Visual Design

### Map

- Simplified world map with territory boundaries
- Each territory drawn as a filled polygon region
- Territory color indicates owning player
- Army count displayed as number in each territory
- Highlighted borders for selected/attackable territories

### Player Colors

1. Red
2. Blue
3. Green
4. Yellow
5. Purple

### UI Elements

- World map fills main window area
- Status bar at bottom: current player, phase, instructions
- Dice display area for combat resolution
- Resizable window with double-buffered rendering
- Sound effects for dice rolls, conquests, and reinforcement

### Input

- Mouse click to select territories
- Keyboard, mouse, and joystick support following Amitk input arbitration
  pattern

## Technical Notes

### Amitk API Usage

- Buffered mode with ami_select double buffering and ami_frametimer
- ami_frect, ami_line, ami_fellipse for map drawing
- ami_fontsiz scaled to window for territory labels
- Menu via ami_menu with ami_menurec linked list
- Sound via ami_opensynthout/ami_noteon/ami_noteoff
- Events: ami_etframe, ami_etmoumovg, ami_etmouba, ami_etmenus, ami_etresize
- Resize: ami_sizbufg with recalculated map coordinates

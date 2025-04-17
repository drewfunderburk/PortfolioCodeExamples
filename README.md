# PortfolioCodeExamples

[![Email](assets/images/Email.png)](mailto:drewfunderburkbusiness@gmail.com)
[![LinkedIn](assets/images/LinkedIn.png)](https://www.linkedin.com/in/drew-funderburk)
[![GitHub](assets/images/Github.png)](https://github.com/drewfunderburk)
[![Portfolio](assets/images/Portfolio.png)](drewfunderburk.github.io/portfolio)

> This repository serves as a convenience for viewing snippets of my code without needing access to private repositories, as many of the things I work on are not public.

### [Unreal Engine 5](Examples/Unreal%20C%2B%2B)
##### [MultiplayerGameInstance](Examples/Unreal%20C%2B%2B/MultiplayerGameInstance.h)
This game instance is in use in Proximo One, a Sci-Fi FPS currently in development. It serves to facilitate steam multiplayer connections via joining through the friends list, as we did not want to implement server browsing or in game UI for joining friends as of yet.

##### [PlayerBase](Examples/Unreal%20C%2B%2B/PlayerBase.h)
A FPS Character also in use for Proximo One. This class is not yet complete, as I am currently in the process of converting the existing player from Blueprint into C++ and have not yet implemented all of its features. It should, however, give ample insight into my code philosophy.

###### Features:
- Implements a Finite State Machine for handling locomotion states
- A Blocker system to allow anything to "block" a transition from one locomotion state to another or block the use of an ability. For example, muddy terrain can block the player's ability to jump simply by adding a blocker to the list, thus making it unneccesary to expose the inner workings of the player.
- Overridden crouch functionality from Unreal Engine's default implementation to allow for camera smoothing without sacrificing the safety and robust nature of the built-in UE implementation.

##### [CustomCharacterMovementComponent](Examples/Unreal%20C%2B%2B/CustomCharacterMovementComponent.h)
Used by PlayerBase, this component implements a custom movement mode used in the ledge grabbing mechanic to take advantage of the CharacterMovementComponent's client prediction. It implements a linear movement by its acceleration, as it is intended to be used for curve-driven movement.
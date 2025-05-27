## Developing

### How to start 2 game instances in CLion IDE?

1. Go to `Run -> Edit Configurations`
2. Check `Allow multiple instances` under `MatrixGame`
3. Duplicate `MatrixGame` configuration and add `CLIENT1=`
environment variable to the first one and `CLIENT2=` to the second one.
4. Click `+` button and select `Compound`
5. Select both configurations.
6. Select your created `Compound` inside the droplist configuration selector in IDE and click `Run` to start both of them at the same time.

When you start the game with `CLIENT2` environment variable defined
its window is going to be **moved** to the right automatically so the two windows do not screen each other.
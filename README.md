#### Instructions

1. Put dgTool.nds in the root of your 3ds sdmc card.
2. Place the dgTool folder with it's files inside in the root as well.
3. Run the app with the dsiware entry point of your choice. Flashcards will not work - no access to NAND.
4. Make a nand backup (dump) for extra security. Make sure OLD/NEW 3DS at the top of the screen matches your model of 3ds. Only 3ds's with "New" in the title are NEW 3DS.
   Check the firmware indicated below the system model matches your own as well.
5. Select "Downgrade FIRM0 to 10.4" to downgrade your 11.x firm to 10.4, allowing kernel hax.
6. Use 3dsident.3dsx to check if FIRM version is 2.50-11. If not, the firm downgrade was unsuccessfull.

#### General Info

This app only writes to FIRM0, not FIRM1, so it should be safe given your FIRM1 is not corrupt or a9lh'd.
Never use this on anything but 3ds firm 11.0.X-YZ - 11.2.X-YZ. NEVER use this if arm9loaderhax is installed; on any firmware! The result will be 100% bricking.
Remember, if something goes wrong, it's your fault! NAND writing is always a high risk. Read the LICENSE.txt (MIT) included for details.

#### Credits

This was written by an anonymous contributor; I only maintain this repository.

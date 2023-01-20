- Fix void functions ending without return statements (mostly done)
 
- Check all exported apis for wether they only need to be exported in test builds.

    - Maybe find a better solution for testing internal APIs like building with default public visibility for testing. 

- Test idempotency of optimization passes

- Perhaps make DCE pass remove unused instructions, though right now other passes already do this. 

- Fix parser crashes

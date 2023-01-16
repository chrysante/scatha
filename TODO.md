- Fix void functions ending without return statements (mostly done)
 
- Check all exported apis for wether they only need to be exported in test builds.

    - Maybe find a better solution for testing internal APIs like building with default public visibility for testing. 

- Properly deallocate IR-CFG nodes

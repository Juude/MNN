# How to Release @alibaba/mnn

## Prerequisites

1.  **NPM Account**: You need an account on [npmjs.com](https://www.npmjs.com/).
2.  **MNN Build**: The package requires MNN core libraries to be built.

## Important Note on Dependencies

The current `CMakeLists.txt` for this package depends on the MNN source tree structure (specifically `../include`, `../source`, and `../build`).

**Directly publishing this package to npm will result in a broken install for users** unless they are building it within a full MNN repository clone.

To fix this for a public release, you must either:
1.  **Bundle Headers**: Copy required headers from MNN core into `js/include` and update `CMakeLists.txt` to point there.
2.  **Prebuild Binaries**: Use `node-pre-gyp` or `prebuild` to ship compiled binaries, avoiding the need for users to have the MNN source/build environment.

## Publishing (Standard)

If you are publishing to a private registry or have addressed the dependency issues above:

1.  **Login to NPM**:
    ```bash
    npm login
    ```

2.  **Update Version**:
    Update the version in `package.json`:
    ```bash
    npm version patch  # or minor, major
    ```

3.  **Publish**:
    ```bash
    npm publish --access public
    ```

## Local Testing

To verify what will be published (without actually publishing):

```bash
npm pack --dry-run
```

This will show you the file list and package size. Ensure no large test assets or sensitive files are included.

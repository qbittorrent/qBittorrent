nova3 Engine
===

## Development Workflow

0. Prerequisite

   * A Linux-like environment
   * [Python](https://www.python.org/) installed
   * [uv](https://docs.astral.sh/uv/) installed

1. Setup development environment

   1. Setup virtual environment and dependencies
      ```shell
      uv sync
      ```

   2. Activate virtual environment
      ```shell
      source .venv/bin/activate
      ```

2. Run type check

   ```shell
   just check
   ```

3. Run static analyzer

   ```shell
   just lint
   ```

4. Apply formatting

   ```shell
   just format
   ```

## References

* [How to write a search plugin](https://github.com/qbittorrent/search-plugins/wiki/How-to-write-a-search-plugin)
* [just - Command runner](https://just.systems/man/en/)
* [uv - Python package and project manager](https://docs.astral.sh/uv/)

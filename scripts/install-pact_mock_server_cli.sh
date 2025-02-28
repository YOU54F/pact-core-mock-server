#!/usr/bin/env sh

set -e
set -x

MOCK_SERVER_CLI_VERSION="1.0.6" # https://github.com/pact-foundation/pact-core-mock-server/releases

mkdir -p ~/.pact/bin
case "$(uname -s)" in

   Darwin)
     echo '== Installing mock server CLI for Mac OSX =='
     if [ "$(uname -m)" = "arm64" ]; then
        curl -L -o ~/.pact/bin/pact_mock_server_cli.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-macos-aarch64.gz
     else
        curl -L -o ~/.pact/bin/pact_mock_server_cli.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-macos-x86_64.gz
     fi
     gunzip -N -f ~/.pact/bin/pact_mock_server_cli.gz
     chmod +x ~/.pact/bin/pact_mock_server_cli
     ;;

   Linux)
     echo '== Installing mock server CLI for Linux =='
     if [ "$(uname -m)" = "aarch64" ]; then
      curl -L -o ~/.pact/bin/pact_mock_server_cli.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-linux-aarch64.gz
     else
      curl -L -o ~/.pact/bin/pact_mock_server_cli.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-linux-x86_64.gz
     fi
     gunzip -N -f ~/.pact/bin/pact_mock_server_cli.gz
     chmod +x ~/.pact/bin/pact_mock_server_cli
     ;;

   CYGWIN*|MINGW32*|MSYS*|MINGW*)
     echo '== Installing mock server CLI for MS Windows =='
     if [ "$(uname -m)" = "aarch64" ]; then
      curl -L -o ~/.pact/bin/pact_mock_server_cli.exe.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-windows-aarch64.exe.gz
     else
      curl -L -o ~/.pact/bin/pact_mock_server_cli.exe.gz https://github.com/pact-foundation/pact-core-mock-server/releases/download/pact_mock_server_cli-v${MOCK_SERVER_CLI_VERSION}/pact_mock_server_cli-windows-x86_64.exe.gz
     fi
     gunzip -N -f ~/.pact/bin/pact_mock_server_cli.exe.gz
     chmod +x ~/.pact/bin/pact_mock_server_cli.exe
     ;;

   *)
     echo "ERROR: $(uname -s) is not a supported operating system"
     exit 1
     ;;
esac
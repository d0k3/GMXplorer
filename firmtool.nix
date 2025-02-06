{ lib, buildPythonPackage, fetchFromGitHub, pythonOlder, setuptools, pip, pycryptodome }:

buildPythonPackage rec {
  pname = "firmtool";
  version = "1.4";
  format = "setuptools";

  disabled = pythonOlder "3.2";

  src = fetchFromGitHub {
    owner = "TuxSH";
    repo = "firmtool";
    rev = "v${version}";
    hash = "sha256-7fvMeHbbkOEIutLiZt+zU8ZNBgrX6WRq66NIOyDgRV0=";
  };

  propagatedBuildInputs = [ setuptools pip pycryptodome ];

  pythonImportsCheck = [ "firmtool" ];
}

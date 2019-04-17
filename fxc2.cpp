#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <direct.h>
#include <stdio.h>
#include <getopt.h>
#include <string>
#include <wchar.h>


#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
typedef HRESULT(__stdcall *pCompileFromFileg)(LPCWSTR,
					      const D3D_SHADER_MACRO[],
					      ID3DInclude*,
					      LPCSTR,
					      LPCSTR,
					      UINT,
					      UINT,
					      ID3DBlob**,
					      ID3DBlob**);

void print_usage_arg() {
  // https://msdn.microsoft.com/en-us/library/windows/desktop/bb509709(v=vs.85).aspx
  printf("You have specified an argument that is not handled by fxc2\n");
  printf("This isn't a sign of disaster, odds are it will be very easy to add support for this argument.\n");
  printf("Review the meaning of the argument in the real fxc program, and then add it into fxc2.\n");
  exit(1);
}
void print_usage_missing(const char* arg) {
  printf("fxc2 is missing the %s argument. We expected to receive this, and it's likely things ", arg);
  printf("will not work correctly without it. Review fxc2 and make sure things will work.\n");
  exit(1);
}
void print_usage_toomany() {
  printf("You specified multiple input files. We did not expect to receive this, and aren't prepared to handle ");
  printf("multiple input files. You'll have to edit the source to behave the way you want.\n");
  exit(1);
}

int main(int argc, char* argv[])
{
  // ====================================================================================
  // Process Command Line Arguments

  int verbose = 1;

  char* model = NULL;
  wchar_t* inputFile = NULL;
  char* entryPoint = NULL;
  char* variableName = NULL;
  char* outputFile = NULL;
  int numDefines = 1;
  D3D_SHADER_MACRO* defines = new D3D_SHADER_MACRO[numDefines];
  defines[numDefines-1].Name = NULL;
  defines[numDefines-1].Definition = NULL;

  int i, c;
  static struct option longOptions[] =
  {
    /* These options set a flag. */
    {"nologo", no_argument,       &verbose, 0},
    {0, 0, 0, 0}
  };

  while (1) {
    D3D_SHADER_MACRO* newDefines;

    int optionIndex = 0;
    c = getopt_long_only (argc, argv, "T:E:D:V:F:",
                     longOptions, &optionIndex);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        //printf ("option -nologo (quiet)\n");
        //Technically, this is any flag we define in longOptions
        break;
      case 'T':
        model = strdup(optarg);
        if(verbose) {
          printf ("option -T (Shader Model/Profile) with arg %s\n", optarg);
        }
        break;
      case 'E':
        entryPoint = strdup(optarg);
        if(verbose) {
          printf ("option -E (Entry Point) with arg %s\n", optarg);
        }
        break;
      case 'D':
        numDefines++;
        //Copy the old array into the new array, but put the new definition at the beginning
        newDefines = new D3D_SHADER_MACRO[numDefines];
        for(i=1; i<numDefines; i++)
          newDefines[i] = defines[i-1];
        delete[] defines;
        defines = newDefines;
        defines[0].Name = strdup(optarg);
        defines[0].Definition = "1";
        if(verbose) {
          printf ("option -D with arg %s\n", optarg);
        }
        break;

      case 'V':
        switch(optarg[0])
        {
          case 'n':
            variableName = strdup(&optarg[1]);
            if(verbose) {
              printf ("option -Vn (Variable Name) with arg %s\n", &optarg[1]);
            }
            break;
          case 'i':
            if(verbose) {
              printf("option -Vi (Output include process details) acknowledged but ignored.\n");
            }
            break;
          default:
            print_usage_arg();
            break;
        }
        break;
      case 'F':
        switch(optarg[0])
        {
          case 'h':
            outputFile = strdup(&optarg[1]);
            if(verbose) {
              printf ("option -Fh (Output File) with arg %s\n", &optarg[1]);
            }
            break;
          default:
            print_usage_arg();
            break;
        }
        break;

      case '?':
      default:
        print_usage_arg();
        break;
    }
  }

  if (optind < argc) {
    inputFile = new wchar_t[strlen(argv[optind])+1];
    mbstowcs(inputFile, argv[optind], strlen(argv[optind])+1);
    if(verbose) {
      wprintf(L"input file: %ls\n", inputFile);
    }

    optind++;
    if(optind < argc) {
      print_usage_toomany();
    }
  }

  if(inputFile == NULL)
    print_usage_missing("inputFile");
  if(model == NULL)
    print_usage_missing("model");
  if(entryPoint == NULL)
    print_usage_missing("entryPoint");
  if(defines == NULL)
    print_usage_missing("defines");
  if(variableName == NULL)
    print_usage_missing("variableName");
  if(outputFile == NULL)
    print_usage_missing("outputFile");

  // ====================================================================================
  // Shader Compilation

  //Find the WINDOWS dll
  char dllPath[ MAX_PATH ];
  int bytes = GetModuleFileName(NULL, dllPath, MAX_PATH);
  if(bytes == 0) {
    printf("Could not retrieve the directory of the running executable.\n");
    return 1;
  }
  //Fill rest of the buffer with NULLs
  memset(dllPath + bytes, '\0', MAX_PATH - bytes);
  //Copy the dll location over top fxc2.exe
  strcpy(strrchr(dllPath, '\\') + 1, "d3dcompiler_47.dll");
  
  HMODULE h = LoadLibrary(dllPath);
  if(h == NULL) {
    printf("Error: could not load d3dcompiler_47.dll from %s\n", dllPath);
    return 1;
  }

  pCompileFromFileg ptr = (pCompileFromFileg)GetProcAddress(h, "D3DCompileFromFile");
  if(ptr == NULL) {
    printf("Error: could not get the address of D3DCompileFromFile.\n");
    return 1;
  }

  HRESULT hr;
  ID3DBlob* output = NULL;
  ID3DBlob* errors = NULL;

  if(verbose) {
    printf("Calling D3DCompileFromFile(\n");
    
    wprintf(L"\t %ls,\n", inputFile);
    
    printf("\t");
    for(i=0; i<numDefines-1; i++)
      printf(" %s=%s", defines[i].Name, defines[i].Definition);
    printf(",\n");

    printf("\t D3D_COMPILE_STANDARD_FILE_INCLUDE,\n");

    printf("\t %s,\n", entryPoint);

    printf("\t %s,\n", model);

    printf("\t 0,\n");
    printf("\t 0,\n");
    printf("\t &output,\n");
    printf("\t &errors);\n");
  }

  /*
  HRESULT WINAPI D3DCompileFromFile(
  in      LPCWSTR pFileName,
  in_opt  const D3D_SHADER_MACRO pDefines,
  in_opt  ID3DInclude pInclude,
  in      LPCSTR pEntrypoint,
  in      LPCSTR pTarget,
  in      UINT Flags1,
  in      UINT Flags2,
  out     ID3DBlob ppCode,
  out_opt ID3DBlob ppErrorMsgs
  );
  */
  hr = ptr(
    inputFile,
    defines,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,
    entryPoint,
    model,
    0,
    0,
    &output,
    &errors
    );

  // ====================================================================================
  // Output (or errors)

  if (FAILED(hr)) {
   if (errors) {
    char* error = (char*)errors->GetBufferPointer();
    printf("Got an error (%i) while compiling:\n%s\n", hr, error);
    errors->Release();
   } else {
     printf("Got an error (%i) while compiling, but no error message from the function.\n", hr);

     LPSTR messageBuffer = nullptr;
     size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
     printf("Windows Error Message: %s\n", messageBuffer);
     LocalFree(messageBuffer);
   }

   if (output)
     output->Release();

   return hr;
  } else {
    char * outString = (char*)output->GetBufferPointer();
    int len = output->GetBufferSize();

    FILE* f;
    errno_t err = fopen_s(&f, outputFile, "w");

    fprintf(f, "const signed char %s[] =\n{\n", entryPoint);
    for (i = 0; i < len; i++) {
     fprintf(f, "%4i", outString[i]);
     if (i != len - 1)
       fprintf(f, ",");
     if (i % 6 == 5)
       fprintf(f, "\n");
    }

    fprintf(f, "\n};\n");
    fclose(f);

    if(verbose) {
      printf("Wrote %i bytes of shader output to %s\n", len, outputFile);
    }
  }

  return 0;
}

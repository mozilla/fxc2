/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <direct.h>
#include <stdio.h>
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

struct ProfilePrefix {
  const char* name;
  const char* prefix;
};

static const ProfilePrefix g_profilePrefixTable[] = {
  { "ps_2_0", "g_ps20"},
  { "ps_2_a", "g_ps21"},
  { "ps_2_b", "g_ps21"},
  { "ps_2_sw", "g_ps2ff"},
  { "ps_3_0", "g_ps30"},
  { "ps_3_sw", "g_ps3ff"},

  { "vs_1_1", "g_vs11"},
  { "vs_2_0", "g_vs20"},
  { "vs_2_a", "g_vs21"},
  { "vs_2_sw", "g_vs2ff"},
  { "vs_3_0", "g_vs30"},
  { "vs_3_sw", "g_vs3ff"},

  { NULL, NULL}
};

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

bool parseOpt( const char* option, int argc, const char** argv, int* index, char** argumentOption )
{
    assert(option != NULL);
    if (!index || *index >= argc) {
      return false;
    }
    const char* argument = argv[*index];
    if (argument[0] == '-' || argument[0] == '/')
      argument++;
    else
      return false;

    size_t optionSize = strlen(option);
    if (strncmp(argument, option, optionSize) != 0) {
      return false;
    }

    if (argumentOption) {
      argument += optionSize;
      if (*argument == '\0') {
        *index += 1;
        if (*index >= argc) {
          printf("Error: missing required argument for option %s\n", option);
          return false;
        }
        *argumentOption = strdup(argv[*index]);
      } else {
        *argumentOption = strdup(argument);
      }
    }
    *index += 1;
    return true;
}

int main(int argc, const char* argv[])
{
  // ====================================================================================
  // Process Command Line Arguments

  int verbose = 1;

  char* model = NULL;
  wchar_t* inputFile = NULL;
  char* entryPoint = NULL;
  char* variableName = NULL;
  char* outputFile = NULL;
  char* defineOption = NULL;
  int numDefines = 1;
  D3D_SHADER_MACRO* defines = new D3D_SHADER_MACRO[numDefines];
  defines[numDefines-1].Name = NULL;
  defines[numDefines-1].Definition = NULL;

  int index = 1;
  while (1) {
    D3D_SHADER_MACRO* newDefines;

    /* Detect the end of the options. */
    if (index >= argc)
      break;

    if (parseOpt("nologo", argc, argv, &index, NULL)) {
      continue;
    } else if (parseOpt("T", argc, argv, &index, &model)) {
      if(verbose) {
        printf ("option -T (Shader Model/Profile) with arg '%s'\n", model);
      }
        continue;
    } else if (parseOpt("E", argc, argv, &index, &entryPoint)) {
      if(verbose) {
        printf ("option -E (Entry Point) with arg '%s'\n", entryPoint);
      }
      continue;
    } else if (parseOpt("D", argc, argv, &index, &defineOption)) {
      numDefines++;
      //Copy the old array into the new array, but put the new definition at the beginning
      newDefines = new D3D_SHADER_MACRO[numDefines];
      for(int i=1; i<numDefines; i++)
        newDefines[i] = defines[i-1];
      delete[] defines;
      defines = newDefines;
      defines[0].Name = defineOption;
      defines[0].Definition = "1";
      if(verbose) {
        printf ("option -D with arg %s\n", defineOption);
      }
      continue;
    } else if (parseOpt("Vn", argc, argv, &index, &variableName)) {
      if(verbose) {
        printf ("option -Vn (Variable Name) with arg '%s'\n", variableName);
      }
      continue;
    } else if (parseOpt("Vi", argc, argv, &index, NULL)) {
      if(verbose) {
        printf("option -Vi (Output include process details) acknowledged but ignored.\n");
      }
      continue;
    } else if (parseOpt("Fh", argc, argv, &index, &outputFile)) {
      if(verbose) {
        printf ("option -Fh (Output File) with arg %s\n", outputFile);
      }
      continue;
    } else if (parseOpt("?", argc, argv, &index, NULL)) {
      print_usage_arg();
      continue;
    } else {
      if (!inputFile)
      {
        inputFile = new wchar_t[strlen(argv[index])+1];
        mbstowcs(inputFile, argv[index], strlen(argv[index])+1);
        if(verbose) {
          wprintf(L"input file: %ls\n", inputFile);
        }
        index += 1;
      } else {
        print_usage_toomany();
        return 1;
      }
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
  if(outputFile == NULL)
    print_usage_missing("outputFile");

  //Default output variable name
  if (variableName == NULL) {
      const char* prefix = "g";
      for (int i = 0; g_profilePrefixTable[i].name != NULL; i++) {
          if (strcmp(g_profilePrefixTable[i].name, model) == 0) {
              prefix = g_profilePrefixTable[i].prefix;
              break;
          }
      }
      variableName = (char*)malloc(strlen(prefix) + strlen(entryPoint) + 2);
      sprintf(variableName, "%s_%s", prefix, entryPoint);
  }

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
    for(int i=0; i<numDefines-1; i++)
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

    fprintf(f, "const signed char %s[] =\n{\n", variableName);
    for (int i = 0; i < len; i++) {
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

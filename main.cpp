#include "stdafx.h"

_NT_BEGIN

ULONG gdwThreadId;

struct T2 
{
	PVOID Left, Right;
};

T2* BuildThree(HWND hwndTV, TVINSERTSTRUCTW* tvis, PVOID pNode, T2* p, PCWSTR lor)
{
	HTREEITEM hParent = tvis->hParent;

	if (0 < swprintf_s(tvis->itemex.pszText, tvis->itemex.cchTextMax, L"[%x] %s%p", tvis->itemex.iImage++, lor, pNode))
	{
		if (tvis->hParent = TreeView_InsertItem(hwndTV, tvis))
		{
			PVOID Left = p->Left, Right = p++->Right;

			if (Left)
			{
				p = BuildThree(hwndTV, tvis, Left, p, L"L ");
			}

			if (Right)
			{
				p = BuildThree(hwndTV, tvis, Right, p, L"R ");
			}
		}
	}

	tvis->hParent = hParent;

	return p;
}

void ShowThree(PVOID pNode, T2* p, HFONT hFont)
{
	if (HWND hwndTV = CreateWindowExW(0, WC_TREEVIEW, L"***", WS_OVERLAPPEDWINDOW|
		TVS_LINESATROOT|TVS_HASLINES|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP|TVS_TRACKSELECT|TVS_EDITLABELS, 
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0))
	{
		SendMessageW(hwndTV, WM_SETFONT, (WPARAM)hFont, 0);

		WCHAR buf[0x40];

		TVINSERTSTRUCTW tvis = { TVI_ROOT, TVI_LAST, { TVIF_TEXT, 0, 0, 0, buf, _countof(buf) } };

		BuildThree(hwndTV, &tvis, pNode, p, L"R ");

		ShowWindow(hwndTV, SW_SHOW);
	}
}

struct EnumData 
{
	ULONG n;
	LONG m;
	HWND* phwnd;
};

BOOL CALLBACK EnumThreadWndProc( HWND hwnd, LPARAM lParam)
{
	if (IsWindowVisible(hwnd))
	{
		//DestroyWindow(hwnd);
		reinterpret_cast<EnumData*>(lParam)->n++;
		if (0 < reinterpret_cast<EnumData*>(lParam)->m--)
		{
			*reinterpret_cast<EnumData*>(lParam)->phwnd++ = hwnd;
		}
	}
	return TRUE;
}

ULONG CALLBACK UiThread(PVOID)
{
	NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
	{
		ncm.lfCaptionFont.lfHeight = -ncm.iMenuHeight;
		ncm.lfCaptionFont.lfWeight = FW_NORMAL;
		ncm.lfCaptionFont.lfQuality = CLEARTYPE_QUALITY;
		ncm.lfCaptionFont.lfPitchAndFamily = FIXED_PITCH|FF_MODERN;
		wcscpy(ncm.lfCaptionFont.lfFaceName, L"Courier New");

		if (HFONT hFont = CreateFontIndirect(&ncm.lfCaptionFont))
		{
			MSG msg;
			while (0 < GetMessageW(&msg, 0, 0, 0))
			{
				switch (msg.message)
				{
				case WM_NULL:
					ShowThree((PVOID)msg.wParam, (T2*)msg.lParam, hFont);
					delete [] (T2*)msg.lParam;
					break;
				default:
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}
			}

			EnumData ed {};

			while (EnumThreadWindows(GetCurrentThreadId(), EnumThreadWndProc, (LPARAM)&ed), ed.n)
			{
				if (ed.phwnd)
				{
					if (ed.n)
					{
						do 
						{
							DestroyWindow(*--ed.phwnd);
						} while (--ed.n);
					}
					break;
				}

				ed.phwnd = (HWND*)alloca((ed.m = ed.n) * sizeof(HWND));
				ed.n = 0;
			}

			DeleteObject(hFont);
		}
	}

	FreeLibraryAndExitThread((HMODULE)&__ImageBase, 0);
}

HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags) 
{
	HMODULE hmod;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PCWSTR)&__ImageBase, &hmod))
	{
		if (HANDLE hThread = CreateThread(0, 0, UiThread, 0, 0, &gdwThreadId))
		{
			CloseHandle(hThread);
		}
	}

	*Version = DEBUG_EXTENSION_VERSION(1, 0);
	*Flags = 0; 
	return S_OK;
}

void CALLBACK DebugExtensionUninitialize()
{
	if (gdwThreadId) PostThreadMessageW(gdwThreadId, WM_QUIT, 0, 0);
}

HRESULT CALLBACK DebugExtensionCanUnload()
{
	return S_OK;
}

void CALLBACK DebugExtensionUnload()
{
	//DbgPrint("DebugExtensionUnload\n");
}

struct CTX 
{
	T2* p;
	IDebugControl* pDebugControl;
	IDebugDataSpaces* pDataSpace;
	ULONG NumberGenericTableElements, i, Level;

	void DumpNode(RTL_BALANCED_LINKS* Parent, RTL_BALANCED_LINKS* pNode, PCSTR prefix);
};

void CTX::DumpNode(RTL_BALANCED_LINKS* Parent, RTL_BALANCED_LINKS* pNode, PCSTR prefix)
{
	if (!NumberGenericTableElements)
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "!! NumberGenericTableElements\n");

		return ;
	}

	--NumberGenericTableElements;

	RTL_BALANCED_LINKS node;

	ULONG BytesRead;
	HRESULT hr = pDataSpace->ReadVirtual((ULONG_PTR)pNode, &node, sizeof(node), &BytesRead);

	if (0 > hr && BytesRead != sizeof(node))
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "!! ReadVirtual(%p)=%x [%x]\n", pNode, hr, BytesRead);

		return ;
	}

	if (node.Parent != Parent)
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "!! %p->Parent(%p) != Parent(%p)\n", pNode, node.Parent, Parent);
	}

	if (0 <= *prefix)
	{
		pDebugControl->Output(DEBUG_OUTPUT_NORMAL, "%s[%x] %p< %p, %p > [%x]\n",
			prefix, i++, pNode, node.LeftChild, node.RightChild, Level);

		--prefix;
	}

	p->Left = node.LeftChild;
	p++->Right = node.RightChild;

	++Level;

	if (node.LeftChild)
	{
		DumpNode(pNode, node.LeftChild, prefix);
	}

	if (node.RightChild)
	{
		DumpNode(pNode, node.RightChild, prefix);
	}

	--Level;
}

void DumpTree(ULONG64 Offset, IDebugControl* pDebugControl, IDebugDataSpaces* pDataSpace)
{
	pDebugControl->Output(DEBUG_OUTPUT_NORMAL, "Check AvlTree at %p\n", Offset);

	RTL_AVL_TABLE table;
	ULONG BytesRead;
	HRESULT hr = pDataSpace->ReadVirtual(Offset, &table, sizeof(table), &BytesRead);

	if (0 > hr && BytesRead != sizeof(table))
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "!! ReadVirtual(%I64x)=%x [%x]\n", Offset, hr, BytesRead);

		return ;
	}

	ULONG NumberGenericTableElements = table.NumberGenericTableElements;
	RTL_BALANCED_LINKS *RightChild = table.BalancedRoot.RightChild;

	pDebugControl->Output(DEBUG_OUTPUT_NORMAL,
		"real root       = %p\n"
		"NumberElements  = %x\n"
		"TableContext    = %p\n"
		"CompareRoutine  = %p\n"
		"AllocateRoutine = %p\n"
		"FreeRoutine     = %p\n",
		RightChild,
		NumberGenericTableElements,
		table.TableContext, table.CompareRoutine, table.AllocateRoutine, table.FreeRoutine);
	
	if (table.BalancedRoot.Parent != &reinterpret_cast<RTL_AVL_TABLE*>((ULONG_PTR)Offset)->BalancedRoot)
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, 
			"&BalancedRoot != BalancedRoot.Parent [%p]\n", table.BalancedRoot.Parent);
	}

	if (table.BalancedRoot.LeftChild)
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, 
			"table.BalancedRoot.LeftChild != 0 [%p]\n", table.BalancedRoot.LeftChild);
	}

	if ((RightChild != 0) ^ (NumberGenericTableElements != 0))
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "RightChild ^ NumberGenericTableElements\n");
	}

	if (RightChild && NumberGenericTableElements)
	{
		char prefix[64];
		RtlFillMemoryUlong(prefix, sizeof(prefix), '\t\t\t\t');
		prefix[sizeof(prefix)-1] = 0;
		*prefix = -1;

		if (T2* p = new T2[NumberGenericTableElements])
		{
			CTX ctx { p, pDebugControl, pDataSpace, table.NumberGenericTableElements };

			ctx.DumpNode(&reinterpret_cast<RTL_AVL_TABLE*>((ULONG_PTR)Offset)->BalancedRoot, 
				RightChild, prefix + sizeof(prefix)-1);

			if (!gdwThreadId || !PostThreadMessageW(gdwThreadId, WM_NULL, (WPARAM)RightChild, (LPARAM)p))
			{
				delete [] p;
			}
		}
	}
}

void CALLBACK rtlavl_ex(IDebugClient* pDebugClient, PCSTR args)
{
	IDebugControl* pDebugControl;
	IDebugDataSpaces* pDataSpace;

	if (0 <= pDebugClient->QueryInterface(IID_PPV_ARGS(&pDebugControl)))
	{
		pDebugControl->Output(DEBUG_OUTPUT_ERROR, "\"%s\"\n", args);
		ULONG64 Offset = _strtoui64(args, const_cast<PSTR*>(&args), 16);

		if (*args == '`')
		{
			Offset = (Offset << 32) | _strtoui64(args + 1, const_cast<PSTR*>(&args), 16);
		}

		if (*args)
		{
			pDebugControl->Output(DEBUG_OUTPUT_ERROR, "Invalid args(... \"%s\")\n", args);
		}
		else
		{
			if (0 <= pDebugClient->QueryInterface(IID_PPV_ARGS(&pDataSpace)))
			{
				DumpTree(Offset, pDebugControl, pDataSpace);
				pDataSpace->Release();
			}
		}

		pDebugControl->Release();
	}
}

_NT_END
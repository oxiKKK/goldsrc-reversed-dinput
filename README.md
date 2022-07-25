# :hammer_and_wrench: GoldSource reverse-engineered DInput code

This is an original reverse-engineered **DInput** code for the **GoldSource** engine. This code is now obsolete for over 15 years and it isn't used anywhere inside the engine.

The source code included contains only raw code and therefore cannot be compiled. 

This was done for **educational purposes only**.

# Useful information

The code only exists on the windows build of the engine (on the linux version it's completely stripped away). 

The _DInput_ code was probably used around _2000-2007_ and after that, the usage was stripped away (using the code). However the original code still remained inside the binary to be reversed, it just wasn't used properly. 

The GS engine, while it is for _DSound_, _D3D_ graphics or _DInput_, uses **DirectX 7**. The SDK for that version available can be found at [github.com/oxiKKK/dx7sdk](https://github.com/oxiKKK/dx7sdk). 

# Naming

The original function names are completely **forbidden**. There's no evidence of the original naming of individual functions, so *DInput_* was chosen as a prefix.

While reversing this code, **03 SourceEngine leak** as well as the old **Quake** code was used as a reference for the names and such. The *03 leak* contains class called *CDInput* which contains most of the original code with only small changes made to it. So that was used as well.

# Why does this code even exist in the original game?

Even tho that the code is 'being used' in some places, the initialization routine for the entire *DInput* code isn't called anywhere (i.e. *DInput_Initialize()* routine). 

And even when this function is called (for example inside *Host_Init()*), then after *SetMouseEnable()* executes, the game will crash, because of the **g_pMouse** object being suddenly changed to a *nullptr* by *DInput_DeactivateMouse()*. So the code is half-finished. This is why there's evidence that this code was even never used.

# Traces in the engine

There are exacly two places where the _DInput_ code is being called. One of them being *_Host_Frame()* function and the other *SetMouseEnable()* function.

## _Host_Frame()
![_Host_Frame()](https://i.imgur.com/VLFxQgj.png)

## SetMouseEnable()
![SetMouseEnable()](https://i.imgur.com/rrhgEY3.png)

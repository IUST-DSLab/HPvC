
void RemoveItemFromArray(int index,int* array,int arraySize )
{
	int i = 0;
	// index must be little than arraySize
	if (index >= arraySize)
		return;
	for (i = index; i < arraySize - 1; i++)
		array[i] = array[i + 1];
}
#include "LensReaderAnalogueLiveLinkSourceFactory.h"
#include "LensReaderAnalogueLiveLinkSource.h"
#include "LensReaderAnalogueLiveLink.h"

#define LOCTEXT_NAMESPACE "ULensReaderAnalogueLiveLinkSourceFactory"

FText ULensReaderAnalogueLiveLinkSourceFactory::GetSourceDisplayName() const
{
    return LOCTEXT("SourceDisplayName", "Lens Reader Analogue");
}

FText ULensReaderAnalogueLiveLinkSourceFactory::GetSourceTooltip() const
{
    return LOCTEXT("SourceTooltip", "Creates a connection to a LensReaderAnalogue Server");
}

TSharedPtr<SWidget> ULensReaderAnalogueLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
    return SNew(LensReaderAnalogueLiveLinkSourceEditor)
        .OnOkClicked(LensReaderAnalogueLiveLinkSourceEditor::FOnOkClicked::CreateUObject(this, &ULensReaderAnalogueLiveLinkSourceFactory::OnOkClicked, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> ULensReaderAnalogueLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
/*
    FIPv4Endpoint DeviceEndPoint;
    if (!FIPv4Endpoint::Parse(InConnectionString, DeviceEndPoint))
    {
        return TSharedPtr<ILiveLinkSource>();
    }
*/
    return MakeShared<FLensReaderAnalogueLiveLinkSource>(FText::FromString(InConnectionString));
}

void ULensReaderAnalogueLiveLinkSourceFactory::OnOkClicked(FText InEndpoint, FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
    InOnLiveLinkSourceCreated.ExecuteIfBound(MakeShared<FLensReaderAnalogueLiveLinkSource>(InEndpoint), InEndpoint.ToString());
}

#undef LOCTEXT_NAMESPACE
